#include "FullEngineCoopClientPresentationPassiveUi.h"
#include "FullEngineCoopClientPresentationPassiveUiRosterRaster.h"

#include <Multiplayer/CoopTacticalProtocol.h>

#ifndef JA2_PASSIVE_UI_DATA_FREE_TEST
#include "FullEngineCoopClientPresentationPassiveUiNativeShade.h"
#include "FullEngineCoopClientPresentationPassiveUiRaster.h"
#include "FullEngineCoopClientPresentationPassiveUiRosterTextPlan.h"
#include "Font Control.h"
#include "Font.h"
#include "MemMan.h"
#include "Utilities.h"
#include "himage.h"
#include "line.h"
#include "local.h"
#include "vobject.h"
#include "vobject_blitters.h"
#include "vsurface.h"
#include "render_palette_registry.h"
#endif

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>

namespace
{
constexpr std::uint8_t FixedDayRadarShade = 0;
constexpr std::int32_t TacticalCellPixels = TacticalWorldCellSize;

bool ContainsActor(const TacticalEntityId* actors, std::size_t count,
	TacticalEntityId sought) noexcept
{
	for (std::size_t index = 0; index < count; ++index)
		if (actors[index] == sought) return true;
	return false;
}

bool IsRosterActor(const TacticalActorSnapshot& actor,
	const TacticalWorldDimensions& dimensions) noexcept
{
	return actor.active && actor.inSector && dimensions.contains(actor.grid) &&
		actor.level >= 0 && actor.level <= 1 && actor.direction < 8 &&
		(actor.presentation.flags & TacticalActorRenderPosePresent) != 0;
}

bool ValidAssignmentList(const TacticalWorldSnapshot& snapshot,
	const TacticalEntityId* assignedActors, std::size_t assignedActorCount,
	TacticalEntityId selectedActor) noexcept
{
	if (assignedActorCount >
			CoopSession::MaximumCoopTacticalAssignedActors ||
		(assignedActorCount != 0 && assignedActors == nullptr) ||
		(selectedActor.valid() &&
		 !ContainsActor(assignedActors, assignedActorCount, selectedActor)))
		return false;
	bool haveTeam = false;
	std::uint8_t team = 0;
	for (std::size_t index = 0; index < assignedActorCount; ++index)
	{
		if (!assignedActors[index].valid() ||
			(index != 0 &&
			 !(assignedActors[index - 1] < assignedActors[index])))
			return false;
		const TacticalActorSnapshot* const actor =
			snapshot.find(assignedActors[index]);
		if (actor == nullptr ||
			!IsRosterActor(*actor, snapshot.dimensions()) ||
			(haveTeam && actor->team != team))
			return false;
		team = actor->team;
		haveTeam = true;
	}
	return true;
}

FullEngineCoopClientPassiveRosterEntry CopyRosterEntry(
	const TacticalActorSnapshot& actor) noexcept
{
	FullEngineCoopClientPassiveRosterEntry entry;
	entry.actor = actor.id;
	entry.team = actor.team;
	entry.profile = actor.profile;
	entry.bodyType = actor.presentation.bodyType;
	entry.portrait = actor.presentation.portrait;
	entry.displayNameUtf16 = actor.presentation.displayNameUtf16;
	entry.stance = actor.stance;
	entry.actionPoints = actor.actionPoints;
	entry.life = actor.life;
	entry.maximumLife = actor.maximumLife;
	entry.breath = actor.breath;
	entry.maximumBreath = actor.maximumBreath;
	entry.primaryHand = actor.loadout.primaryHand;
	entry.secondaryHand = actor.loadout.secondaryHand;
	entry.interruptActionEligible = actor.interruptActionEligible;
	return entry;
}

const FullEngineCoopClientPassiveRosterEntry* FindAvailableActor(
	const std::vector<FullEngineCoopClientPassiveRosterEntry>& actors,
	TacticalEntityId sought) noexcept
{
	const auto found = std::lower_bound(actors.begin(), actors.end(), sought,
		[](const FullEngineCoopClientPassiveRosterEntry& entry,
			TacticalEntityId id) { return entry.actor < id; });
	return found != actors.end() && found->actor == sought
		? &*found : nullptr;
}

std::size_t RosterWindowStart(
	const std::vector<FullEngineCoopClientPassiveRosterEntry>& roster,
	TacticalEntityId selectedActor) noexcept
{
	if (!selectedActor.valid()) return 0;
	for (std::size_t index = 0; index < roster.size(); ++index)
		if (roster[index].actor == selectedActor)
			return (index /
				FullEngineCoopClientPassiveUiRosterVisibleSlots) *
				FullEngineCoopClientPassiveUiRosterVisibleSlots;
	return 0;
}

bool SamePanelAsset(const FullEngineCoopClientPassiveUiLayout& left,
	const FullEngineCoopClientPassiveUiLayout& right) noexcept
{
	return std::strcmp(FullEngineCoopClientPassiveTeamPanelAsset(left),
		FullEngineCoopClientPassiveTeamPanelAsset(right)) == 0;
}

#ifndef JA2_PASSIVE_UI_DATA_FREE_TEST
bool LoadPanelResource(const char* path, std::uint32_t& handle,
	void*) noexcept
{
	if (path == nullptr) return false;
	VOBJECT_DESC description{};
	description.fCreateFlags = VOBJECT_CREATE_FROMFILE;
	if (!FilenameForBPP(path, description.ImageFile)) return false;
	return AddVideoObject(&description, &handle) != FALSE;
}

bool LoadRadarResource(const char* path, std::uint32_t& handle,
	void*) noexcept
{
	if (path == nullptr) return false;
	const std::size_t length = std::strlen(path);
	if (length >= SGPFILENAME_LEN) return false;
	VOBJECT_DESC description{};
	description.fCreateFlags = VOBJECT_CREATE_FROMFILE;
	std::memcpy(description.ImageFile, path, length + 1);
	return AddVideoObject(&description, &handle) != FALSE;
}

bool ShadeRadarResource(std::uint32_t handle, std::uint8_t shade,
	void*) noexcept
{
	if (shade != FixedDayRadarShade) return false;
	HVOBJECT object = nullptr;
	if (!GetVideoObject(&object, handle) || object == nullptr) return false;
	const FullEngineCoopClientPassiveRadarShadeServices services{
		[](const SGPPaletteEntry* palette, void*) noexcept -> PIXEL* {
			try
			{
				return Create16BPPPaletteShaded(
					const_cast<SGPPaletteEntry*>(palette),
					255, 255, 255, FALSE);
			}
			catch (...)
			{
				return nullptr;
			}
		},
		[](PIXEL* palette, void*) noexcept {
			if (palette == nullptr) return;
			UnregisterLegacyRenderPalette(palette);
			MemFree(palette);
		},
		nullptr};
	return InstallFullEngineCoopClientPassiveRadarDayShade(
		object, services) ==
		FullEngineCoopClientPassiveRadarShadeResult::Success;
}

void ReleaseResource(std::uint32_t handle, void*) noexcept
{
	(void)DeleteVideoObjectFromIndex(handle);
}

bool BlitResource(std::uint32_t handle, std::int32_t x,
	std::int32_t y, void*) noexcept
{
	return BltVideoObjectFromIndex(FRAME_BUFFER, handle, 0, x, y,
		VO_BLT_SRCTRANSPARENCY, nullptr) != FALSE;
}

PIXEL MarkerPixel(FullEngineCoopClientPassiveUiColor color) noexcept
{
	switch (color)
	{
		case FullEngineCoopClientPassiveUiColor::Camera:
			return Get16BPPColor(FROMRGB(0, 255, 0));
		case FullEngineCoopClientPassiveUiColor::Friendly:
			return Get16BPPColor(FROMRGB(160, 220, 255));
		case FullEngineCoopClientPassiveUiColor::FriendlyRoof:
			return Get16BPPColor(FROMRGB(80, 110, 150));
		case FullEngineCoopClientPassiveUiColor::Assigned:
			return Get16BPPColor(FROMRGB(0, 255, 0));
		case FullEngineCoopClientPassiveUiColor::AssignedRoof:
			return Get16BPPColor(FROMRGB(0, 150, 0));
		case FullEngineCoopClientPassiveUiColor::Hostile:
			return Get16BPPColor(FROMRGB(255, 0, 0));
		case FullEngineCoopClientPassiveUiColor::HostileRoof:
			return Get16BPPColor(FROMRGB(150, 0, 0));
		case FullEngineCoopClientPassiveUiColor::Selected:
			return Get16BPPColor(FROMRGB(255, 255, 0));
		case FullEngineCoopClientPassiveUiColor::SelectedRoof:
			return Get16BPPColor(FROMRGB(150, 150, 0));
		case FullEngineCoopClientPassiveUiColor::Target:
			return Get16BPPColor(FROMRGB(255, 80, 255));
		case FullEngineCoopClientPassiveUiColor::TargetRoof:
			return Get16BPPColor(FROMRGB(170, 40, 170));
		case FullEngineCoopClientPassiveUiColor::DoorClosed:
			return Get16BPPColor(FROMRGB(70, 180, 255));
		case FullEngineCoopClientPassiveUiColor::DoorOpen:
			return Get16BPPColor(FROMRGB(255, 165, 45));
		case FullEngineCoopClientPassiveUiColor::DoorSelected:
			return Get16BPPColor(FROMRGB(255, 255, 0));
		case FullEngineCoopClientPassiveUiColor::Count:
			break;
	}
	return Get16BPPColor(FROMRGB(255, 255, 255));
}

PIXEL RosterPixel(FullEngineCoopClientPassiveRosterColor color) noexcept
{
	switch (color)
	{
		case FullEngineCoopClientPassiveRosterColor::PortraitBackground:
			return Get16BPPColor(FROMRGB(8, 14, 18));
		case FullEngineCoopClientPassiveRosterColor::PortraitSilhouette:
			return Get16BPPColor(FROMRGB(82, 92, 98));
		case FullEngineCoopClientPassiveRosterColor::BodyBadge:
			return Get16BPPColor(FROMRGB(0, 180, 0));
		case FullEngineCoopClientPassiveRosterColor::SelectedBorder:
			return Get16BPPColor(FROMRGB(255, 220, 0));
		case FullEngineCoopClientPassiveRosterColor::MeterBackground:
			return Get16BPPColor(FROMRGB(28, 28, 28));
		case FullEngineCoopClientPassiveRosterColor::Life:
			return Get16BPPColor(FROMRGB(220, 20, 20));
		case FullEngineCoopClientPassiveRosterColor::Breath:
			return Get16BPPColor(FROMRGB(35, 105, 235));
		case FullEngineCoopClientPassiveRosterColor::HandBorder:
			return Get16BPPColor(FROMRGB(92, 86, 68));
		case FullEngineCoopClientPassiveRosterColor::HandOccupied:
			return Get16BPPColor(FROMRGB(34, 39, 34));
		case FullEngineCoopClientPassiveRosterColor::Condition:
			return Get16BPPColor(FROMRGB(20, 190, 30));
		case FullEngineCoopClientPassiveRosterColor::Jammed:
			return Get16BPPColor(FROMRGB(255, 35, 20));
	}
	return Get16BPPColor(FROMRGB(255, 255, 255));
}

template <std::size_t Capacity>
struct PreparedRosterTextRun
{
	HVOBJECT font = nullptr;
	std::array<std::uint16_t, Capacity> glyphs{};
	std::array<std::uint32_t, Capacity> widths{};
	std::size_t count = 0;
	std::int32_t x = 0;
	std::int32_t y = 0;
	SGPRect clip{};
};

struct PreparedRosterText
{
	std::array<PreparedRosterTextRun<
		FullEngineCoopClientPassiveRosterNameTextCapacity>,
		FullEngineCoopClientPassiveUiRosterVisibleSlots> names{};
	std::array<PreparedRosterTextRun<
		FullEngineCoopClientPassiveRosterActionPointTextCapacity>,
		FullEngineCoopClientPassiveUiRosterVisibleSlots> actionPoints{};
	std::array<bool, FullEngineCoopClientPassiveUiRosterVisibleSlots>
		selected{};
	std::size_t visibleSlotCount = 0;
};

struct PreparedStatusFeedbackText
{
	PreparedRosterTextRun<
		FullEngineCoopClientPresentationControlFeedbackTextCapacity> run;
	PreparedRosterTextRun<16> inventoryHint;
	FullEngineCoopClientPassiveUiRect background;
	FullEngineCoopClientPresentationCommandFeedbackState state =
		FullEngineCoopClientPresentationCommandFeedbackState::None;
	bool controlActive = false;
};

bool ResolveRosterGlyph(HVOBJECT font, CHAR16 character,
	INT16& glyph) noexcept
{
	if (font == nullptr) return false;
	if (TryGetIndex(character, &glyph) && glyph >= 0 &&
		static_cast<std::uint16_t>(glyph) < font->usNumberOfObjects)
		return true;
	return character != L'?' && TryGetIndex(L'?', &glyph) && glyph >= 0 &&
		static_cast<std::uint16_t>(glyph) < font->usNumberOfObjects;
}

template <std::size_t TextCapacity, std::size_t RunCapacity>
bool PrepareRosterTextRun(HVOBJECT font,
	const std::array<std::uint16_t, TextCapacity>& text, std::size_t count,
	const FullEngineCoopClientPassiveUiRect& alignment,
	const FullEngineCoopClientPassiveUiRect& clip,
	std::int32_t bitmapFontHeight, bool topAligned,
	PreparedRosterTextRun<RunCapacity>& output) noexcept
{
	if (font == nullptr || font->pETRLEObject == nullptr ||
		font->usNumberOfObjects == 0 || bitmapFontHeight <= 0 ||
		!alignment.valid() || !clip.valid() || count > TextCapacity ||
		count > RunCapacity)
		return false;
	PreparedRosterTextRun<RunCapacity> candidate;
	candidate.font = font;
	candidate.count = count;
	std::int64_t width = 0;
	for (std::size_t index = 0; index < count; ++index)
	{
		INT16 glyph = -1;
		if (!ResolveRosterGlyph(
				font, static_cast<CHAR16>(text[index]), glyph))
			return false;
		candidate.glyphs[index] = static_cast<std::uint16_t>(glyph);
		candidate.widths[index] = GetWidth(font, glyph);
		width += candidate.widths[index];
		if (width > (std::numeric_limits<std::int32_t>::max)()) return false;
	}
	FullEngineCoopClientPassiveUiPoint origin;
	if (!PositionFullEngineCoopClientPassiveRosterTextRun(alignment,
			static_cast<std::int32_t>(width), bitmapFontHeight,
			topAligned, origin) ||
		static_cast<std::int64_t>(origin.x) + width >
			(std::numeric_limits<std::int32_t>::max)())
		return false;
	candidate.x = origin.x;
	candidate.y = origin.y;
	candidate.clip = {clip.left, clip.top, clip.right, clip.bottom};
	output = candidate;
	return true;
}

bool PrepareRosterText(const FullEngineCoopClientPassiveUiModel& model,
	const FullEngineCoopClientPassiveUiRect& panel,
	PreparedRosterText& output) noexcept
{
	if (!gfFontsInit || BLOCKFONT2 < 0 || BLOCKFONT2 >= MAX_FONTS ||
		TINYFONT1 < 0 || TINYFONT1 >= MAX_FONTS ||
		!IsFontLoaded(BLOCKFONT2) || !IsFontLoaded(TINYFONT1))
		return false;
	HVOBJECT const nameFont = GetFontObject(BLOCKFONT2);
	HVOBJECT const actionPointFont = GetFontObject(TINYFONT1);
	if (nameFont == nullptr || actionPointFont == nullptr) return false;
	const auto bitmapHeight = [](HVOBJECT font) noexcept -> std::int32_t {
		return static_cast<std::int32_t>(font->pETRLEObject[0].usHeight) +
			font->pETRLEObject[0].sOffsetY;
	};
	const std::int32_t nameFontHeight = bitmapHeight(nameFont);
	const std::int32_t actionPointFontHeight = bitmapHeight(actionPointFont);

	FullEngineCoopClientPassiveRosterTextPlan plan;
	if (!BuildFullEngineCoopClientPassiveRosterTextPlan(
			model, panel, plan))
		return false;
	PreparedRosterText candidate;
	candidate.visibleSlotCount = plan.visibleSlotCount;
	for (std::size_t slot = 0; slot < plan.visibleSlotCount; ++slot)
	{
		const auto& source = plan.slots[slot];
		if (!PrepareRosterTextRun(nameFont, source.name,
				source.nameLength, source.nameAlignment, source.nameClip,
				nameFontHeight, false,
				candidate.names[slot]) ||
			!PrepareRosterTextRun(actionPointFont, source.actionPoints,
				source.actionPointLength, source.actionPointAlignment,
				source.actionPointClip, actionPointFontHeight, true,
				candidate.actionPoints[slot]))
			return false;
		candidate.selected[slot] = source.selected;
	}
	output = candidate;
	return true;
}

bool PrepareStatusFeedbackText(
	const FullEngineCoopClientPassiveUiModel& model,
	const FullEngineCoopClientPassiveUiRect& panel,
	PreparedStatusFeedbackText& output) noexcept
{
	if (!gfFontsInit || TINYFONT1 < 0 || TINYFONT1 >= MAX_FONTS ||
		!IsFontLoaded(TINYFONT1))
		return false;
	HVOBJECT const font = GetFontObject(TINYFONT1);
	if (font == nullptr || font->pETRLEObject == nullptr) return false;
	const std::int32_t fontHeight =
		static_cast<std::int32_t>(font->pETRLEObject[0].usHeight) +
		font->pETRLEObject[0].sOffsetY;
	if (fontHeight <= 0 || panel.top < fontHeight + 3) return false;
	const FullEngineCoopClientPassiveUiRect strip{
		panel.left, panel.top - fontHeight - 3, panel.right, panel.top};
	const FullEngineCoopClientPassiveUiRect alignment{
		strip.left + 2, strip.top + 1, strip.right - 86, strip.bottom - 1};
	const FullEngineCoopClientPassiveUiRect feedbackClip{
		strip.left, strip.top, strip.right - 86, strip.bottom};
	if (!strip.valid() || !alignment.valid()) return false;

	PreparedStatusFeedbackText candidate;
	// Keep "I: inventory" discoverable in the normal native tactical view.
	// Reserve its own clipped region, so receipts cannot draw over the hint.
	constexpr std::array<std::uint16_t, 16> inventoryHint{
		'I', ':', ' ', 'i', 'n', 'v', 'e', 'n', 't', 'o', 'r', 'y', 0};
	const FullEngineCoopClientPassiveUiRect hintAlignment{
		strip.right - 82, strip.top + 1, strip.right - 2, strip.bottom - 1};
	if (!PrepareRosterTextRun(font, inventoryHint, 12, hintAlignment, hintAlignment,
		fontHeight, true, candidate.inventoryHint)) return false;
	const auto commandState = model.commandFeedback.feedback.state;
	if (model.controlFeedback.feedback.mode !=
		FullEngineCoopClientPresentationControlMode::None ||
		(model.controlFeedback.textLength != 0 &&
		 (commandState == FullEngineCoopClientPresentationCommandFeedbackState::None ||
		  commandState == FullEngineCoopClientPresentationCommandFeedbackState::Applied)))
	{
		if (!PrepareRosterTextRun(font, model.controlFeedback.utf16,
				model.controlFeedback.textLength, alignment, feedbackClip,
				fontHeight, true, candidate.run))
			return false;
		candidate.controlActive = true;
	}
	else if (!PrepareRosterTextRun(font, model.commandFeedback.utf16,
			model.commandFeedback.textLength, alignment, feedbackClip,
			fontHeight, true, candidate.run))
		return false;
	candidate.background = strip;
	candidate.state = model.commandFeedback.feedback.state;
	output = candidate;
	return true;
}

template <std::size_t Capacity>
bool DrawPreparedRosterTextRun(PIXEL* pixels, std::uint32_t pitchBytes,
	const PreparedRosterTextRun<Capacity>& run,
	PIXEL foreground, PIXEL shadow) noexcept
{
	if (pixels == nullptr || run.font == nullptr ||
		run.font->pETRLEObject == nullptr || run.count > Capacity)
		return false;
	std::int64_t cursor = run.x;
	SGPRect clip = run.clip;
	for (std::size_t index = 0; index < run.count; ++index)
	{
		if (cursor < (std::numeric_limits<std::int32_t>::min)() ||
			cursor > (std::numeric_limits<std::int32_t>::max)() ||
			!Blt8BPPDataTo16BPPBufferMonoShadowClip(pixels, pitchBytes,
				run.font, static_cast<std::int32_t>(cursor), run.y,
				run.glyphs[index], &clip, foreground, 0, shadow))
			return false;
		cursor += run.widths[index];
	}
	return true;
}

bool RenderRosterText(const PreparedRosterText& prepared,
	PIXEL* pixels, std::uint32_t pitchBytes) noexcept
{
	const PIXEL ordinary = Get16BPPColor(FROMRGB(190, 190, 180));
	const PIXEL selected = Get16BPPColor(FROMRGB(255, 230, 35));
	const PIXEL shadow = Get16BPPColor(FROMRGB(12, 12, 12));
	for (std::size_t slot = 0; slot < prepared.visibleSlotCount; ++slot)
	{
		const PIXEL foreground = prepared.selected[slot]
			? selected : ordinary;
		if (!DrawPreparedRosterTextRun(pixels, pitchBytes,
				prepared.names[slot], foreground, shadow) ||
			!DrawPreparedRosterTextRun(pixels, pitchBytes,
				prepared.actionPoints[slot], foreground, shadow))
			return false;
	}
	return true;
}

bool FillPixelRectangle(PIXEL* pixels, std::uint32_t pitchBytes,
	std::int32_t surfaceWidth, std::int32_t surfaceHeight,
	const FullEngineCoopClientPassiveUiRect& rectangle,
	PIXEL color) noexcept
{
	if (pixels == nullptr || !rectangle.valid() || rectangle.left < 0 ||
		rectangle.top < 0 || rectangle.right > surfaceWidth ||
		rectangle.bottom > surfaceHeight ||
		pitchBytes % sizeof(PIXEL) != 0 ||
		static_cast<std::uint64_t>(surfaceWidth) * sizeof(PIXEL) > pitchBytes)
		return false;
	const std::size_t stride = pitchBytes / sizeof(PIXEL);
	for (std::int32_t y = rectangle.top; y < rectangle.bottom; ++y)
	{
		PIXEL* const row = pixels + static_cast<std::size_t>(y) * stride;
		std::fill(row + rectangle.left, row + rectangle.right, color);
	}
	return true;
}

PIXEL CommandFeedbackPixel(
	FullEngineCoopClientPresentationCommandFeedbackState state) noexcept
{
	using State = FullEngineCoopClientPresentationCommandFeedbackState;
	switch (state)
	{
		case State::None:
			return Get16BPPColor(FROMRGB(150, 150, 145));
		case State::LocalFailure:
			return Get16BPPColor(FROMRGB(255, 70, 55));
		case State::Submitted:
		case State::Accepted:
			return Get16BPPColor(FROMRGB(255, 225, 40));
		case State::Applied:
			return Get16BPPColor(FROMRGB(40, 235, 70));
		case State::Rejected:
			return Get16BPPColor(FROMRGB(255, 70, 55));
		case State::Discarded:
		case State::Cancelled:
			return Get16BPPColor(FROMRGB(255, 155, 45));
	}
	return Get16BPPColor(FROMRGB(255, 255, 255));
}

bool RenderStatusFeedbackText(
	const PreparedStatusFeedbackText& prepared,
	PIXEL* pixels, std::uint32_t pitchBytes,
	std::int32_t surfaceWidth, std::int32_t surfaceHeight) noexcept
{
	const PIXEL background = Get16BPPColor(FROMRGB(5, 7, 8));
	const PIXEL shadow = Get16BPPColor(FROMRGB(0, 0, 0));
	return FillPixelRectangle(pixels, pitchBytes, surfaceWidth, surfaceHeight,
			prepared.background, background) &&
		DrawPreparedRosterTextRun(pixels, pitchBytes, prepared.run,
			prepared.controlActive
				? Get16BPPColor(FROMRGB(90, 220, 255))
				: CommandFeedbackPixel(prepared.state), shadow) &&
		DrawPreparedRosterTextRun(pixels, pitchBytes, prepared.inventoryHint,
			Get16BPPColor(FROMRGB(185, 190, 175)), shadow);
}

bool RenderOverlay(const FullEngineCoopClientPassiveUiModel& model,
	const FullEngineCoopClientPassiveUiLayout& layout,
	const FullEngineCoopClientPassiveUiProjection& projection,
	void*) noexcept
{
	if (!layout.valid() || !projection.valid() || !model.dimensions.valid())
		return false;
	PreparedRosterText preparedText;
	PreparedStatusFeedbackText preparedFeedback;
	if (!PrepareRosterText(model, layout.panel, preparedText) ||
		!PrepareStatusFeedbackText(model, layout.panel, preparedFeedback))
		return false;
	UINT32 pitch = 0;
	UINT8* const pixels = LockVideoSurface(FRAME_BUFFER, &pitch);
	if (pixels == nullptr) return false;
	bool success = false;
	try
	{
		FullEngineCoopClientPassiveUiPixelColors radarColors{};
		for (std::size_t index = 0; index < radarColors.size(); ++index)
			radarColors[index] = MarkerPixel(
				static_cast<FullEngineCoopClientPassiveUiColor>(index));
		FullEngineCoopClientPassiveRosterPixelColors rosterColors{};
		for (std::size_t index = 0; index < rosterColors.size(); ++index)
			rosterColors[index] = RosterPixel(
				static_cast<FullEngineCoopClientPassiveRosterColor>(index));
		success = RasterFullEngineCoopClientPassiveUiOverlay(model,
			projection, pixels, pitch, SCREEN_WIDTH, SCREEN_HEIGHT,
			radarColors) &&
			RasterFullEngineCoopClientPassiveUiRoster(model, layout.panel,
				pixels, pitch, SCREEN_WIDTH, SCREEN_HEIGHT, rosterColors) &&
			RenderRosterText(preparedText,
				reinterpret_cast<PIXEL*>(pixels), pitch) &&
			RenderStatusFeedbackText(preparedFeedback,
				reinterpret_cast<PIXEL*>(pixels), pitch,
				SCREEN_WIDTH, SCREEN_HEIGHT);
	}
	catch (...)
	{
		success = false;
	}
	UnLockVideoSurface(FRAME_BUFFER);
	return success;
}

FullEngineCoopClientPassiveUiServices DefaultServices() noexcept
{
	return {LoadPanelResource, LoadRadarResource, ShadeRadarResource,
		ReleaseResource, BlitResource, RenderOverlay, nullptr,
		MakeFullEngineCoopClientPortraitNativeServices()};
}
#else
FullEngineCoopClientPassiveUiServices DefaultServices() noexcept
{
	return {};
}
#endif

bool TruncateFinite(double value, std::int32_t& output) noexcept
{
	if (!std::isfinite(value) ||
		value < static_cast<double>((std::numeric_limits<std::int32_t>::min)()) ||
		value > static_cast<double>((std::numeric_limits<std::int32_t>::max)()))
		return false;
	// The native radar projects through INT16 casts. Preserve its
	// truncation-toward-zero rule while keeping the wider checked domain here.
	output = static_cast<std::int32_t>(value);
	return true;
}

void ResetControlFeedback(
	FullEngineCoopClientPassiveUiModel& model) noexcept
{
	// This is the canonical None plan produced by the pure planner: an empty,
	// terminated status line retaining only the resulting local selection.
	model.controlFeedback = {};
	model.controlFeedback.feedback.selectedActor = model.selectedActor;
	for (auto& marker : model.markers) marker.targeted = false;
	for (auto& door : model.doors) door.selected = false;
}

bool ProjectGridMarker(std::int32_t grid, std::int32_t markerExtent,
	const TacticalWorldDimensions& dimensions,
	const FullEngineCoopClientPassiveUiProjection& projection,
	FullEngineCoopClientPassiveUiPoint& output) noexcept
{
	if (!projection.valid() || !dimensions.contains(grid) ||
		markerExtent <= 0 ||
		projection.radar.right - projection.radar.left < markerExtent ||
		projection.radar.bottom - projection.radar.top < markerExtent)
		return false;
	const std::int64_t column = grid % dimensions.columns;
	const std::int64_t row = grid / dimensions.columns;
	const std::int64_t cellX = column * TacticalCellPixels;
	const std::int64_t cellY = row * TacticalCellPixels;
	const std::int64_t screenX =
		static_cast<std::int64_t>(projection.markerOriginX) +
		2 * cellX - 2 * cellY;
	const std::int64_t screenY =
		static_cast<std::int64_t>(projection.markerOriginY) + cellX + cellY;
	std::int32_t x = 0;
	std::int32_t y = 0;
	if (!TruncateFinite(projection.radar.left +
		static_cast<double>(screenX) * projection.scaleX, x) ||
		!TruncateFinite(projection.radar.top +
		static_cast<double>(screenY) * projection.scaleY, y))
		return false;
	x = (std::max)(projection.radar.left,
		(std::min)(x, projection.radar.right - markerExtent));
	y = (std::max)(projection.radar.top,
		(std::min)(y, projection.radar.bottom - markerExtent));
	output = {x, y};
	return true;
}
}

bool FullEngineCoopClientPassiveUiLayout::valid() const noexcept
{
	return screenWidth > 0 && screenHeight > 0 && panel.valid() &&
		radar.valid() && panel.right <= screenWidth &&
		panel.bottom <= screenHeight && radar.right <= screenWidth &&
		radar.bottom <= screenHeight &&
		panel.right - panel.left >=
			FullEngineCoopClientPassiveUiRosterMinimumPanelWidth &&
		panel.bottom - panel.top >=
			FullEngineCoopClientPassiveUiRosterMinimumPanelHeight;
}

bool FullEngineCoopClientPassiveUiProjection::valid() const noexcept
{
	return radar.valid() && std::isfinite(scaleX) &&
		std::isfinite(scaleY) && scaleX > 0.0 && scaleY > 0.0 &&
		viewportWidth > 0 && viewportHeight > 0;
}

FullEngineCoopClientPassiveUiModelResult
BuildFullEngineCoopClientPassiveUiModel(
	const TacticalWorldSnapshot& snapshot,
	const TacticalEntityId* assignedActors,
	std::size_t assignedActorCount,
	TacticalEntityId selectedActor,
	FullEngineCoopClientPassiveUiModel& output) noexcept
{
	using Result = FullEngineCoopClientPassiveUiModelResult;
	if (snapshot.epoch() == 0 || !snapshot.dimensions().valid() ||
		!snapshot.sector().loaded ||
		!IsValidTacticalSectorSnapshot(snapshot.sector()) ||
		snapshot.actors().size() >
			CoopSession::MaximumCoopTacticalSnapshotActors ||
		snapshot.doors().size() >
			CoopSession::MaximumCoopTacticalSnapshotDoors)
		return Result::InvalidSnapshot;
	if (!ValidAssignmentList(snapshot, assignedActors, assignedActorCount,
		selectedActor))
		return Result::InvalidAssignments;

	try
	{
		FullEngineCoopClientPassiveUiModel candidate;
		candidate.worldGeneration = snapshot.epoch();
		candidate.dimensions = snapshot.dimensions();
		candidate.mapAssetKey = snapshot.sector().mapAssetKey;
		candidate.selectedActor = selectedActor;
		candidate.markers.reserve(snapshot.actors().size());
		candidate.doors.reserve(snapshot.doors().size());
		candidate.availableActors.reserve(snapshot.actors().size());
		candidate.roster.reserve(assignedActorCount);
		for (const TacticalActorSnapshot& actor : snapshot.actors())
		{
			if (!IsRosterActor(actor, candidate.dimensions))
				continue;
			if (candidate.markers.size() >=
				CoopSession::MaximumCoopTacticalSnapshotActors)
				return Result::TooManyMarkers;
			const bool assigned = ContainsActor(
				assignedActors, assignedActorCount, actor.id);
			FullEngineCoopClientPassiveMarker marker;
			marker.actor = actor.id;
			marker.grid = actor.grid;
			marker.level = actor.level;
			marker.hostile = actor.hostileToPlayerTeam;
			marker.kind = assigned
				? FullEngineCoopClientPassiveMarkerKind::Assigned
				: actor.hostileToPlayerTeam
					? FullEngineCoopClientPassiveMarkerKind::Hostile
					: FullEngineCoopClientPassiveMarkerKind::Friendly;
			marker.selected = assigned && actor.id == selectedActor;
			candidate.markers.push_back(marker);
			candidate.availableActors.push_back(CopyRosterEntry(actor));
		}
		for (std::size_t index = 0; index < snapshot.doors().size(); ++index)
		{
			const TacticalDoorSnapshot& door = snapshot.doors()[index];
			if (!candidate.dimensions.contains(door.baseGrid) ||
				door.structureId == 0 ||
				(index != 0 && snapshot.doors()[index - 1].baseGrid >=
					door.baseGrid))
				return Result::InvalidSnapshot;
			candidate.doors.push_back(
				{door.baseGrid, door.structureId, door.open, false});
		}
		for (std::size_t index = 0; index < assignedActorCount; ++index)
		{
			const auto* const available = FindAvailableActor(
				candidate.availableActors, assignedActors[index]);
			if (available == nullptr)
				return Result::InvalidAssignments;
			candidate.roster.push_back(*available);
			candidate.roster.back().selected =
				available->actor == selectedActor;
		}
		candidate.rosterFirstVisible =
			RosterWindowStart(candidate.roster, selectedActor);
		if (!BuildFullEngineCoopClientPresentationCommandFeedbackTextPlan(
				{candidate.worldGeneration, 0, false, {}},
				candidate.commandFeedback))
			return Result::InvalidSnapshot;
		FullEngineCoopClientPresentationControlFeedbackInput controlInput;
		controlInput.currentWorldGeneration = candidate.worldGeneration;
		for (std::size_t index = 0; index < assignedActorCount; ++index)
			controlInput.assignedActors[index] = assignedActors[index];
		controlInput.assignedActorCount = assignedActorCount;
		controlInput.selectedActor = selectedActor;
		if (!BuildFullEngineCoopClientPresentationControlFeedbackTextPlan(
				snapshot, controlInput, candidate.controlFeedback))
			return Result::InvalidSnapshot;
		output = std::move(candidate);
		return Result::Success;
	}
	catch (...)
	{
		return Result::AllocationFailure;
	}
}

bool ProjectFullEngineCoopClientPassiveMarker(
	const FullEngineCoopClientPassiveMarker& marker,
	const TacticalWorldDimensions& dimensions,
	const FullEngineCoopClientPassiveUiProjection& projection,
	FullEngineCoopClientPassiveUiPoint& output) noexcept
{
	return marker.level >= 0 && marker.level <= 1 &&
		ProjectGridMarker(marker.grid, 2, dimensions, projection, output);
}

bool ProjectFullEngineCoopClientPassiveDoorMarker(
	const FullEngineCoopClientPassiveDoorMarker& marker,
	const TacticalWorldDimensions& dimensions,
	const FullEngineCoopClientPassiveUiProjection& projection,
	FullEngineCoopClientPassiveUiPoint& output) noexcept
{
	return marker.structureId != 0 &&
		ProjectGridMarker(marker.baseGrid, 3, dimensions, projection, output);
}

bool ProjectFullEngineCoopClientPassiveCamera(
	const FullEngineCoopClientPassiveUiProjection& projection,
	FullEngineCoopClientPassiveUiRect& output) noexcept
{
	if (!projection.valid()) return false;
	const double centerX =
		static_cast<double>(projection.radar.left) +
		(static_cast<double>(projection.markerOriginX) +
			2.0 * projection.cameraCenterX -
			2.0 * projection.cameraCenterY) * projection.scaleX;
	const double centerY =
		static_cast<double>(projection.radar.top) +
		(static_cast<double>(projection.markerOriginY) +
			projection.cameraCenterX + projection.cameraCenterY) *
			projection.scaleY;
	const double halfWidth = projection.viewportWidth * projection.scaleX / 2.0;
	const double halfHeight =
		projection.viewportHeight * projection.scaleY / 2.0;
	std::int32_t left = 0;
	std::int32_t top = 0;
	std::int32_t rightInclusive = 0;
	std::int32_t bottomInclusive = 0;
	if (!TruncateFinite(centerX - halfWidth, left) ||
		!TruncateFinite(centerY - halfHeight, top) ||
		!TruncateFinite(centerX + halfWidth, rightInclusive) ||
		!TruncateFinite(centerY + halfHeight, bottomInclusive))
		return false;
	if (static_cast<std::int64_t>(rightInclusive) - left + 1 >=
		static_cast<std::int64_t>(projection.radar.right) -
			projection.radar.left)
	{
		left = projection.radar.left;
		rightInclusive = projection.radar.right - 1;
	}
	else
	{
		left = (std::max)(left, projection.radar.left);
		rightInclusive = (std::min)(rightInclusive,
			projection.radar.right - 1);
	}
	if (static_cast<std::int64_t>(bottomInclusive) - top + 1 >=
		static_cast<std::int64_t>(projection.radar.bottom) -
			projection.radar.top)
	{
		top = projection.radar.top;
		bottomInclusive = projection.radar.bottom - 1;
	}
	else
	{
		top = (std::max)(top, projection.radar.top);
		bottomInclusive = (std::min)(bottomInclusive,
			projection.radar.bottom - 1);
	}
	if (rightInclusive < left || bottomInclusive < top) return false;
	output = {left, top, rightInclusive + 1, bottomInclusive + 1};
	return true;
}

FullEngineCoopClientPassiveUiColor
FullEngineCoopClientPassiveMarkerColor(
	const FullEngineCoopClientPassiveMarker& marker) noexcept
{
	if (marker.targeted)
		return marker.level > 0
			? FullEngineCoopClientPassiveUiColor::TargetRoof
			: FullEngineCoopClientPassiveUiColor::Target;
	if (marker.selected)
		return marker.level > 0
			? FullEngineCoopClientPassiveUiColor::SelectedRoof
			: FullEngineCoopClientPassiveUiColor::Selected;
	switch (marker.kind)
	{
		case FullEngineCoopClientPassiveMarkerKind::Assigned:
			return marker.level > 0
				? FullEngineCoopClientPassiveUiColor::AssignedRoof
				: FullEngineCoopClientPassiveUiColor::Assigned;
		case FullEngineCoopClientPassiveMarkerKind::Hostile:
			return marker.level > 0
				? FullEngineCoopClientPassiveUiColor::HostileRoof
				: FullEngineCoopClientPassiveUiColor::Hostile;
		case FullEngineCoopClientPassiveMarkerKind::Friendly:
			return marker.level > 0
				? FullEngineCoopClientPassiveUiColor::FriendlyRoof
				: FullEngineCoopClientPassiveUiColor::Friendly;
	}
	return FullEngineCoopClientPassiveUiColor::Friendly;
}

const char* FullEngineCoopClientPassiveTeamPanelAsset(
	const FullEngineCoopClientPassiveUiLayout& layout) noexcept
{
	// Passive chrome deliberately has no roster-slot contract yet. Select the
	// deterministic stock six-slot background from the initialized native
	// layout; do not read a cold campaign's squad-size setting or infer its
	// resolution enum from mutable physical window dimensions. JA2's 1280x720
	// family intentionally shifts the radar beyond its nominal 800-wide panel,
	// which remains true when a window manager adjusts the physical surface.
	if (layout.panel.right - layout.panel.left >= 1024 ||
		layout.radar.right > layout.panel.right)
		return "INTERFACE\\bottom_bar_1024x768.sti";
	if (layout.panel.right - layout.panel.left >= 800)
		return "INTERFACE\\bottom_bar_800x600.sti";
	return "INTERFACE\\bottom_bar.sti";
}

bool BuildFullEngineCoopClientPassiveRadarAssetPath(
	const TacticalMapAssetKey& key, char* output,
	std::size_t outputCapacity) noexcept
{
	if (!IsValidTacticalMapAssetKey(key) || output == nullptr ||
		outputCapacity == 0)
		return false;
	std::size_t keyLength = 0;
	while (key.bytes[keyLength] != '\0') ++keyLength;
	const std::size_t stemLength = keyLength - 4;
	constexpr char Prefix[] = "RADARMAPS\\";
	constexpr char Suffix[] = ".STI";
	const std::size_t required = sizeof(Prefix) - 1 + stemLength +
		sizeof(Suffix);
	if (required > outputCapacity || required >
		FullEngineCoopClientPassiveUiAssetPathBytes)
		return false;
	std::array<char, FullEngineCoopClientPassiveUiAssetPathBytes> candidate{};
	std::size_t cursor = 0;
	std::memcpy(candidate.data() + cursor, Prefix, sizeof(Prefix) - 1);
	cursor += sizeof(Prefix) - 1;
	std::memcpy(candidate.data() + cursor, key.bytes.data(), stemLength);
	cursor += stemLength;
	std::memcpy(candidate.data() + cursor, Suffix, sizeof(Suffix));
	std::memcpy(output, candidate.data(), required);
	return true;
}

const char* FullEngineCoopClientPassiveUiResultName(
	FullEngineCoopClientPassiveUiResult result) noexcept
{
	switch (result)
	{
		case FullEngineCoopClientPassiveUiResult::Success: return "success";
		case FullEngineCoopClientPassiveUiResult::InvalidInput:
			return "invalid passive UI input";
		case FullEngineCoopClientPassiveUiResult::ModelFailure:
			return "passive UI model failure";
		case FullEngineCoopClientPassiveUiResult::PanelAssetFailure:
			return "TEAM chrome asset failure";
		case FullEngineCoopClientPassiveUiResult::RadarPathFailure:
			return "radar asset path failure";
		case FullEngineCoopClientPassiveUiResult::RadarAssetFailure:
			return "radar bitmap asset failure";
		case FullEngineCoopClientPassiveUiResult::RadarShadeFailure:
			return "radar day-shade failure";
	}
	return "unknown passive UI result";
}

const char* FullEngineCoopClientPassiveUiRenderResultName(
	FullEngineCoopClientPassiveUiRenderResult result) noexcept
{
	switch (result)
	{
		case FullEngineCoopClientPassiveUiRenderResult::NotReady:
			return "passive UI not ready";
		case FullEngineCoopClientPassiveUiRenderResult::Success:
			return "success";
		case FullEngineCoopClientPassiveUiRenderResult::PanelBlitFailure:
			return "TEAM chrome blit failure";
		case FullEngineCoopClientPassiveUiRenderResult::RadarBlitFailure:
			return "radar bitmap blit failure";
		case FullEngineCoopClientPassiveUiRenderResult::OverlayFailure:
			return "radar/TEAM overlay failure";
	}
	return "unknown passive UI render result";
}

FullEngineCoopClientPresentationPassiveUi::
FullEngineCoopClientPresentationPassiveUi() noexcept
	: FullEngineCoopClientPresentationPassiveUi(DefaultServices())
{
}

FullEngineCoopClientPresentationPassiveUi::
FullEngineCoopClientPresentationPassiveUi(
	FullEngineCoopClientPassiveUiServices services) noexcept
	: services_(services), portraits_(services.portraits)
{
}

FullEngineCoopClientPresentationPassiveUi::~
FullEngineCoopClientPresentationPassiveUi()
{
	teardown();
}

bool FullEngineCoopClientPresentationPassiveUi::servicesValid() const noexcept
{
	return services_.loadPanel != nullptr && services_.loadRadar != nullptr &&
		services_.shadeRadar != nullptr && services_.release != nullptr &&
		services_.blit != nullptr && services_.renderOverlay != nullptr;
}

FullEngineCoopClientPassiveUiResult
FullEngineCoopClientPresentationPassiveUi::synchronize(
	const TacticalWorldSnapshot& snapshot,
	const TacticalEntityId* assignedActors,
	std::size_t assignedActorCount,
	TacticalEntityId selectedActor,
	const FullEngineCoopClientPassiveUiLayout& layout) noexcept
{
	using Result = FullEngineCoopClientPassiveUiResult;
	if (!servicesValid() || !layout.valid()) return Result::InvalidInput;
	FullEngineCoopClientPassiveUiModel candidate;
	if (BuildFullEngineCoopClientPassiveUiModel(snapshot, assignedActors,
		assignedActorCount, selectedActor, candidate) !=
		FullEngineCoopClientPassiveUiModelResult::Success)
		return Result::ModelFailure;

	const bool replaceAssets = !ready_ ||
		model_.mapAssetKey != candidate.mapAssetKey ||
		!SamePanelAsset(layout_, layout);
	if (replaceAssets)
	{
		std::array<char, FullEngineCoopClientPassiveUiAssetPathBytes> radarPath{};
		if (!BuildFullEngineCoopClientPassiveRadarAssetPath(
				candidate.mapAssetKey, radarPath.data(), radarPath.size()))
			return Result::RadarPathFailure;
		std::uint32_t candidatePanel = 0;
		std::uint32_t candidateRadar = 0;
		if (!services_.loadPanel(
				FullEngineCoopClientPassiveTeamPanelAsset(layout),
				candidatePanel, services_.context))
			return Result::PanelAssetFailure;
		if (!services_.loadRadar(
				radarPath.data(), candidateRadar, services_.context))
		{
			services_.release(candidatePanel, services_.context);
			return Result::RadarAssetFailure;
		}
		if (!services_.shadeRadar(
				candidateRadar, FixedDayRadarShade, services_.context))
		{
			services_.release(candidateRadar, services_.context);
			services_.release(candidatePanel, services_.context);
			return Result::RadarShadeFailure;
		}
		releaseHandles();
		panelHandle_ = candidatePanel;
		radarHandle_ = candidateRadar;
		panelLoaded_ = true;
		radarLoaded_ = true;
	}
	model_ = std::move(candidate);
	layout_ = layout;
	synchronizePortraits();
	ready_ = true;
	renderResult_ = FullEngineCoopClientPassiveUiRenderResult::NotReady;
	return Result::Success;
}

void FullEngineCoopClientPresentationPassiveUi::updateSelected(
	TacticalEntityId selectedActor) noexcept
{
	if (!ready_) return;
	bool found = false;
	std::size_t selectedIndex = 0;
	for (std::size_t index = 0; index < model_.roster.size(); ++index)
	{
		auto& entry = model_.roster[index];
		entry.selected = entry.actor == selectedActor;
		if (entry.selected)
		{
			found = true;
			selectedIndex = index;
		}
	}
	for (auto& marker : model_.markers)
	{
		marker.selected = found && marker.kind ==
			FullEngineCoopClientPassiveMarkerKind::Assigned &&
			marker.actor == selectedActor;
	}
	model_.selectedActor = found ? selectedActor : TacticalEntityId{};
	model_.rosterFirstVisible = found
		? (selectedIndex /
			FullEngineCoopClientPassiveUiRosterVisibleSlots) *
			FullEngineCoopClientPassiveUiRosterVisibleSlots
		: 0;
	ResetControlFeedback(model_);
	synchronizePortraits();
}

bool FullEngineCoopClientPresentationPassiveUi::updateAssignments(
	const TacticalEntityId* assignedActors, std::size_t assignedActorCount,
	TacticalEntityId selectedActor) noexcept
{
	if (!ready_ ||
		assignedActorCount >
			CoopSession::MaximumCoopTacticalAssignedActors ||
		(assignedActorCount != 0 && assignedActors == nullptr) ||
		(selectedActor.valid() &&
		 !ContainsActor(assignedActors, assignedActorCount, selectedActor)))
		return false;
	std::uint8_t team = 0;
	bool haveTeam = false;
	for (std::size_t index = 0; index < assignedActorCount; ++index)
	{
		if (!assignedActors[index].valid() ||
			(index != 0 &&
			 !(assignedActors[index - 1] < assignedActors[index])))
			return false;
		const auto* const available = FindAvailableActor(
			model_.availableActors, assignedActors[index]);
		if (available == nullptr ||
			(haveTeam && available->team != team))
			return false;
		team = available->team;
		haveTeam = true;
	}

	std::vector<FullEngineCoopClientPassiveRosterEntry> roster;
	try
	{
		roster.reserve(assignedActorCount);
		for (std::size_t index = 0; index < assignedActorCount; ++index)
		{
			const auto* const available = FindAvailableActor(
				model_.availableActors, assignedActors[index]);
			if (available == nullptr) return false;
			roster.push_back(*available);
			roster.back().selected = available->actor == selectedActor;
		}
	}
	catch (...)
	{
		return false;
	}
	for (auto& marker : model_.markers)
	{
		const bool assigned = ContainsActor(
			assignedActors, assignedActorCount, marker.actor);
		marker.kind = assigned
			? FullEngineCoopClientPassiveMarkerKind::Assigned
			: marker.hostile
				? FullEngineCoopClientPassiveMarkerKind::Hostile
				: FullEngineCoopClientPassiveMarkerKind::Friendly;
		marker.selected = assigned && marker.actor == selectedActor;
	}
	model_.roster = std::move(roster);
	model_.selectedActor = selectedActor;
	model_.rosterFirstVisible =
		RosterWindowStart(model_.roster, selectedActor);
	ResetControlFeedback(model_);
	synchronizePortraits();
	return true;
}

void FullEngineCoopClientPresentationPassiveUi::synchronizePortraits() noexcept
{
	static_assert(FullEngineCoopClientPortraitVisibleSlots ==
		FullEngineCoopClientPassiveUiRosterVisibleSlots, "portrait cache follows the stock roster page");
	std::array<FullEngineCoopClientVisiblePortrait, FullEngineCoopClientPortraitVisibleSlots> visible{};
	if (model_.rosterFirstVisible > model_.roster.size() ||
		model_.rosterFirstVisible % FullEngineCoopClientPassiveUiRosterVisibleSlots != 0)
	{ portraits_.teardown(); return; }
	const auto count = std::min(visible.size(), model_.roster.size() - model_.rosterFirstVisible);
	for (std::size_t slot = 0; slot < count; ++slot)
	{
		const auto& entry = model_.roster[model_.rosterFirstVisible + slot];
		visible[slot] = {entry.actor, entry.portrait};
	}
	(void)portraits_.synchronize(model_.worldGeneration, visible.data(), count);
}

bool FullEngineCoopClientPresentationPassiveUi::updateCommandFeedback(
	const FullEngineCoopClientPresentationCommandFeedbackInput& input)
	noexcept
{
	if (!ready_ || input.currentWorldGeneration != model_.worldGeneration)
		return false;
	FullEngineCoopClientPresentationCommandFeedbackTextPlan candidate;
	if (!BuildFullEngineCoopClientPresentationCommandFeedbackTextPlan(
			input, candidate))
		return false;
	model_.commandFeedback = candidate;
	return true;
}

bool FullEngineCoopClientPresentationPassiveUi::updateControlFeedback(
	const TacticalWorldSnapshot& snapshot,
	const FullEngineCoopClientPresentationControlFeedbackInput& input)
	noexcept
{
	if (!ready_ || snapshot.epoch() != model_.worldGeneration ||
		snapshot.dimensions().columns != model_.dimensions.columns ||
		snapshot.dimensions().rows != model_.dimensions.rows ||
		input.currentWorldGeneration != model_.worldGeneration ||
		input.selectedActor != model_.selectedActor)
		return false;
	FullEngineCoopClientPresentationControlFeedbackTextPlan candidate;
	if (!BuildFullEngineCoopClientPresentationControlFeedbackTextPlan(
			snapshot, input, candidate))
		return false;

	std::size_t targetIndex = model_.markers.size();
	std::size_t doorIndex = model_.doors.size();
	if (candidate.feedback.mode ==
		FullEngineCoopClientPresentationControlMode::Attack)
	{
		for (std::size_t index = 0; index < model_.markers.size(); ++index)
		{
			const auto& marker = model_.markers[index];
			if (marker.actor != candidate.feedback.attackTarget) continue;
			if (!marker.hostile ||
				marker.grid != candidate.feedback.attackTargetGrid ||
				marker.level != candidate.feedback.attackTargetLevel)
				return false;
			targetIndex = index;
			break;
		}
		if (targetIndex == model_.markers.size()) return false;
	}
	if (candidate.feedback.mode ==
		FullEngineCoopClientPresentationControlMode::Door)
	{
		for (std::size_t index = 0; index < model_.doors.size(); ++index)
		{
			const auto& door = model_.doors[index];
			if (door.baseGrid == candidate.feedback.doorBaseGrid &&
				door.structureId == candidate.feedback.doorStructureId &&
				door.open == candidate.feedback.doorOpen)
			{
				doorIndex = index;
				break;
			}
		}
		if (doorIndex == model_.doors.size()) return false;
	}

	for (std::size_t index = 0; index < model_.markers.size(); ++index)
		model_.markers[index].targeted = index == targetIndex;
	for (std::size_t index = 0; index < model_.doors.size(); ++index)
		model_.doors[index].selected = index == doorIndex;
	model_.controlFeedback = candidate;
	return true;
}

FullEngineCoopClientPassiveUiRenderResult
FullEngineCoopClientPresentationPassiveUi::render(
	const FullEngineCoopClientPassiveUiProjection& projection) noexcept
{
	using Result = FullEngineCoopClientPassiveUiRenderResult;
	if (!ready_ || !panelLoaded_ || !radarLoaded_ ||
		!servicesValid() || !projection.valid() ||
		projection.radar.left != layout_.radar.left ||
		projection.radar.top != layout_.radar.top ||
		projection.radar.right != layout_.radar.right ||
		projection.radar.bottom != layout_.radar.bottom)
		return renderResult_ = Result::NotReady;
	if (!services_.blit(panelHandle_, layout_.panel.left,
		layout_.panel.top, services_.context))
		return renderResult_ = Result::PanelBlitFailure;
	if (!services_.blit(radarHandle_, layout_.radar.left,
		layout_.radar.top, services_.context))
		return renderResult_ = Result::RadarBlitFailure;
	if (!services_.renderOverlay(model_, layout_, projection,
			services_.context))
		return renderResult_ = Result::OverlayFailure;
	// The raster pass has already painted a neutral face. Missing optional
	// artwork retains that silhouette instead of taking down the tactical view.
	for (std::size_t slot = 0; slot < FullEngineCoopClientPassiveUiRosterVisibleSlots; ++slot)
	{
		FullEngineCoopClientPassiveRosterSlotGeometry geometry;
		if (BuildFullEngineCoopClientPassiveRosterSlotGeometry(layout_.panel, slot, geometry))
			(void)portraits_.render(slot, geometry.face);
	}
	return renderResult_ = Result::Success;
}

void FullEngineCoopClientPresentationPassiveUi::releaseHandles() noexcept
{
	if (radarLoaded_)
	{
		services_.release(radarHandle_, services_.context);
		radarLoaded_ = false;
		radarHandle_ = 0;
	}
	if (panelLoaded_)
	{
		services_.release(panelHandle_, services_.context);
		panelLoaded_ = false;
		panelHandle_ = 0;
	}
}

void FullEngineCoopClientPresentationPassiveUi::teardown() noexcept
{
	portraits_.teardown();
	releaseHandles();
	model_ = {};
	layout_ = {};
	ready_ = false;
	renderResult_ = FullEngineCoopClientPassiveUiRenderResult::NotReady;
}
