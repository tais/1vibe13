#include "AimWebsiteLayout.h"

#include <cstdlib>
#include <iostream>

namespace
{
using namespace AimWebsiteLayoutModel;

void Require(bool condition, const char* message)
{
	if (condition) return;
	std::cerr << "FAILED: " << message << '\n';
	std::exit(EXIT_FAILURE);
}

bool Is(const Point& point, int x, int y)
{
	return point.x == x && point.y == y;
}

bool Is(const Rect& rect, int x, int y, int width, int height)
{
	return rect.x == x && rect.y == y &&
		rect.width == width && rect.height == height;
}
}

int main()
{
	using namespace AimWebsiteLayoutModel;
	constexpr PageAnchors anchors{0, 0, 111, 46, 19};
	constexpr AimDefaultsLayout aimLegacy =
		MakeAimDefaultsLayout(false, anchors);
	constexpr AimDefaultsLayout aimExpanded =
		MakeAimDefaultsLayout(true, anchors);
	Require(Is(aimLegacy.logo, 260, 49, 203, 51) &&
		Is(aimExpanded.logo, 114, 49, 102, 26),
		"A.I.M. logo drawing and hitboxes select one variant rectangle");
	Require(aimLegacy.backgroundTileCount() == 16 &&
		Is(aimLegacy.backgroundTile(0), 111, 46, 125, 100) &&
		Is(aimLegacy.backgroundTile(3), 486, 46, 125, 100) &&
		Is(aimLegacy.backgroundTile(4), 111, 146, 125, 100) &&
		Is(aimLegacy.backgroundTile(15), 486, 346, 125, 100),
		"A.I.M. background tiles derive from one tested row-major canvas");
	for (std::size_t tile = 0; tile < aimLegacy.backgroundTileCount(); ++tile)
		Require(LaptopLayoutModel::Contains(
			aimLegacy.pageBounds, aimLegacy.backgroundTile(tile)),
			"every A.I.M. background tile stays inside the page bounds");

	constexpr VideoConferenceLayout video =
		MakeVideoConferenceLayout({0, 0, 19});
	Require(Is(video.terminal, 125, 116, 368, 150) &&
		Is(video.face, 133, 143, 96, 86),
		"video terminal and face retain their exact authored geometry");
	Require(Is(video.contractImage, 131, 246) &&
		Is(video.closeButton, 473, 119) &&
		Is(video.name, 132, 121) &&
		Is(video.contractChargeLabel, 132, 234) &&
		Is(video.contractChargeAmount, 132, 247, 98, 12),
		"video artwork and contract text share one terminal-relative layout");
	Require(Is(video.oneTimeFeeOffer.origin, 240, 161) &&
		video.oneTimeFeeOffer.width == 245,
		"campaign-specific offer text stays within the shared terminal model");

	Require(Is(video.contractButtons.at(0), 238, 151) &&
		Is(video.contractButtons.at(2), 238, 197) &&
		Is(video.equipmentButtons.at(1), 360, 174),
		"contract and equipment controls derive from tested vertical strides");
	Require(Is(video.contactButtons.at(0), 238, 178) &&
		Is(video.contactButtons.at(1), 360, 178) &&
		Is(video.authorizationButtons.at(1), 360, 228) &&
		Is(video.unavailableHangUp, 290, 178),
		"contact, authorization, and standalone hang-up controls retain their distinct anchors");
	Require(Is(video.selectionLight(video.contractButtons.at(1), false),
		343, 181) &&
		Is(video.selectionLight(video.contractButtons.at(1), true),
		343, 182),
		"selection lights derive from the same button positions they annotate");

	Require(Is(video.popup, 260, 159, 162, 100) &&
		Is(video.popupButton, 280, 221) &&
		video.popupFirstTextY == 165 &&
		video.talkingTextPopupY == 274,
		"video message overlays retain their exact web-canvas anchors");
	Require(Is(video.titleFrame(0), 331, 401, 74, 21) &&
		Is(video.titleFrame(1), 319, 384, 90, 21) &&
		Is(video.titleFrame(9), 228, 248, 219, 21) &&
		Is(video.titleFrame(18), 125, 96, 365, 21),
		"title-bar animation frames preserve legacy integer interpolation");
	Require(Is(video.titleFrame(100), 125, 96, 365, 21),
		"title-bar animation clamps out-of-range frame requests");

	constexpr VideoConferenceLayout shiftedVideo =
		MakeVideoConferenceLayout({160, 90, 19});
	Require(Is(shiftedVideo.terminal, 285, 206, 368, 150) &&
		Is(shiftedVideo.popup, 420, 249, 162, 100) &&
		Is(shiftedVideo.titleFrame(9), 388, 338, 219, 21),
		"all video-call geometry follows centered-screen offsets");

	constexpr FacialIndexLayout legacy =
		MakeFacialIndexLayout(false, anchors);
	constexpr FacialIndexLayout expanded =
		MakeFacialIndexLayout(true, anchors);
	Require(Is(legacy.pageBounds, 111, 46, 502, 400) &&
		Is(legacy.pageButton, 117, 81, 75, 18),
		"facial-index controls stay inside the fixed Laptop web canvas");
	Require(Is(legacy.memberTitle.origin, 266, 101) &&
		Is(expanded.memberTitle.origin, 266, 84) &&
		legacy.memberTitle.width == 190,
		"facial-index title variants select one typed text area");
	Require(legacy.grid.capacity() == kFacialIndexPageCapacity &&
		Is(legacy.grid.cell(0), 117, 115, 52, 48) &&
		Is(legacy.grid.cell(7), 551, 115, 52, 48) &&
		Is(legacy.grid.cell(8), 117, 176, 52, 48) &&
		Is(legacy.grid.cell(39), 551, 359, 52, 48),
		"facial-index drawing and hitboxes use one row-major grid");
	Require(Is(legacy.grid.face(0), 119, 117, 48, 43) &&
		Is(legacy.grid.nickname(0).origin, 115, 164) &&
		legacy.grid.nickname(0).width == 56 &&
		Is(legacy.grid.status(0).origin, 120, 138) &&
		legacy.grid.status(0).width == 48,
		"face artwork, nickname, and status overlays derive from each portrait cell");
	Require(Is(legacy.help.leftClick.origin, 116, 54) &&
		Is(expanded.help.leftClick.origin, 200, 54) &&
		Is(legacy.help.rightClick.origin, 500, 54),
		"facial-index help text preserves legacy and expanded logo clearances");
	for (std::size_t slot = 0; slot < legacy.grid.capacity(); ++slot)
		Require(LaptopLayoutModel::Contains(
			legacy.pageBounds, legacy.grid.cell(slot)),
			"every facial-index hitbox remains inside the page bounds");

	Require(FacialIndexPageCount(0) == 1 &&
		FacialIndexPageCount(40) == 1 &&
		FacialIndexPageCount(41) == 2 &&
		FacialIndexPageCount(80) == 2 &&
		FacialIndexPageCount(81) == 3 &&
		FacialIndexPageCount(255) == 3,
		"facial-index pagination preserves the three authored page-label families");
	Require(NextFacialIndexPageStart(80, 0) == 40 &&
		NextFacialIndexPageStart(80, 40) == 0 &&
		NextFacialIndexPageStart(100, 40) == 80 &&
		NextFacialIndexPageStart(100, 80) == 0 &&
		PreviousFacialIndexPageStart(100, 0) == 80 &&
		PreviousFacialIndexPageStart(80, 0) == 40,
		"mouse and keyboard navigation share the same wrapping page transitions");
	Require(NormalizeFacialIndexPageStart(80, 20) == 0 &&
		NormalizeFacialIndexPageStart(80, 80) == 0 &&
		FacialIndexVisibleSlotCount(41, 40) == 1 &&
		FacialIndexVisibleSlotCount(100, 80) == 20 &&
		FacialIndexVisibleSlotCount(100, 120) == 0,
		"stale pages and partial final grids cannot expose nonexistent profiles");
	Require(FacialIndexPageTextIndex(40, 0) == 5 &&
		FacialIndexPageTextIndex(80, 0) == 0 &&
		FacialIndexPageTextIndex(80, 40) == 1 &&
		FacialIndexPageTextIndex(100, 0) == 2 &&
		FacialIndexPageTextIndex(100, 40) == 3 &&
		FacialIndexPageTextIndex(100, 80) == 4,
		"page transitions remain aligned with the six localized page labels");

	constexpr auto policyStatement =
		MakePolicyLayout(true, false, anchors);
	constexpr auto policyLegacy =
		MakePolicyLayout(false, false, anchors);
	constexpr auto policyExpanded =
		MakePolicyLayout(false, true, anchors);
	Require(Is(policyStatement.title.origin, 260, 150) &&
		Is(policyLegacy.title.origin, 260, 111) &&
		Is(policyExpanded.title.origin, 260, 84) &&
		policyExpanded.title.width == 203,
		"policy pages select one tested statement, legacy, or expanded title area");
	Require(Is(policyLegacy.tocButtons.at(0), 259, 134, 205, 19) &&
		Is(policyLegacy.tocButtons.at(8), 259, 334, 205, 19) &&
		Is(policyLegacy.tocTextInset, 5, 5),
		"policy table-of-contents drawing and hitboxes share one vertical sequence");
	Require(Is(policyLegacy.menuButtons.at(0), 151, 409) &&
		Is(policyLegacy.menuButtons.at(3), 496, 409) &&
		Is(policyLegacy.agreementButtons.at(0), 261, 369) &&
		Is(policyLegacy.agreementButtons.at(1), 386, 369),
		"policy navigation and agreement controls derive from tested strides");

	constexpr SortLayout sort = MakeSortLayout(anchors);
	Require(Is(sort.pageBounds, 111, 46, 502, 400) &&
		Is(sort.sortPanel, 164, 142, 394, 81) &&
		Is(sort.memberTitle.origin, 164, 124) &&
		sort.memberTitle.width == 394 && Is(sort.sortTitle, 173, 150),
		"A.I.M. Sort panel and headings retain their exact authored coordinates");
	Require(Is(sort.navigationArtwork.at(0), 200, 230, 54, 54) &&
		Is(sort.navigationArtwork.at(2), 200, 350, 54, 54) &&
		Is(sort.navigationText(0), 266, 249) &&
		Is(sort.navigationText(1), 266, 312) &&
		Is(sort.navigationText(2), 266, 370),
		"A.I.M. Sort navigation drawing and hitboxes share one sequence");
	Require(Is(sort.control(0), 173, 176, 10, 10) &&
		Is(sort.control(2), 173, 202, 10, 10) &&
		Is(sort.control(3), 269, 176, 10, 10) &&
		Is(sort.control(11), 461, 202, 10, 10) &&
		Is(sort.control(12), 173, 163, 10, 10) &&
		Is(sort.control(13), 536, 146, 10, 10) &&
		Is(sort.control(14), 536, 159, 10, 10),
		"all fifteen sort lights use one bounded mode-to-control mapping");
	Require(sort.hasControl(14) && !sort.hasControl(15) &&
		Is(sort.control(15), 0, 0, 0, 0),
		"invalid A.I.M. Sort modes cannot index beyond the control model");
	Require(Is(sort.criterionText(0), 186, 178) &&
		Is(sort.criterionText(3), 282, 178) &&
		Is(sort.criterionText(11), 474, 204) &&
		Is(sort.criterionText(12), 186, 165) &&
		Is(sort.criterionHitbox(0, 40), 173, 176, 50, 10) &&
		Is(sort.criterionHitbox(12, 30), 173, 163, 40, 10),
		"sort labels and their input regions derive from the same controls");
	Require(Is(sort.criterionHitbox(0, 1000), 173, 176, 385, 10) &&
		Is(sort.orderHitbox(13, 1000), 164, 146, 382, 10) &&
		LaptopLayoutModel::Contains(
			sort.sortPanel, sort.criterionHitbox(0, 1000)) &&
		LaptopLayoutModel::Contains(
			sort.sortPanel, sort.orderHitbox(13, 1000)),
		"long localized sort labels cannot overflow or underflow the panel hitboxes");
	Require(Is(sort.orderText(13).origin, 432, 147) &&
		Is(sort.orderText(14).origin, 432, 160) &&
		Is(sort.orderHitbox(13, 60), 470, 146, 76, 10) &&
		Is(sort.orderHitbox(14, 40), 490, 159, 56, 10),
		"sort-order text, lights, hitboxes, and invalidation share one layout");

	constexpr PageAnchors shiftedAnchors{160, 90, 271, 136, 19};
	constexpr SortLayout shiftedSort = MakeSortLayout(shiftedAnchors);
	Require(Is(shiftedSort.sortPanel, 324, 232, 394, 81) &&
		Is(shiftedSort.navigationArtwork.at(2), 360, 440, 54, 54) &&
		Is(shiftedSort.control(14), 696, 249, 10, 10),
		"all A.I.M. Sort geometry follows centered-screen translation");

	constexpr ArchiveLayout archive = MakeArchiveLayout(anchors);
	Require(Is(archive.pageBounds, 111, 46, 502, 400) &&
		Is(archive.title.origin, 260, 100) && archive.title.width == 203 &&
		archive.grid.capacity() == kArchivePageCapacity,
		"A.I.M. Archives title and four-by-five capacity retain authored geometry");
	Require(Is(archive.grid.frame(0), 148, 114, 66, 64) &&
		Is(archive.grid.frame(4), 508, 114, 66, 64) &&
		Is(archive.grid.frame(5), 148, 186, 66, 64) &&
		Is(archive.grid.frame(19), 508, 330, 66, 64) &&
		Is(archive.grid.face(0), 152, 118) &&
		Is(archive.grid.hitbox(0), 148, 114, 56, 50) &&
		Is(archive.grid.nickname(0).origin, 153, 169) &&
		archive.grid.nickname(0).width == 56,
		"archive drawing, names, and face hitboxes share one row-major grid");
	for (std::size_t slot = 0; slot < archive.grid.capacity(); ++slot)
		Require(LaptopLayoutModel::Contains(
			archive.pageBounds, archive.grid.frame(slot)),
			"every archive portrait frame stays inside the web canvas");
	Require(Is(archive.pageButton, 311, 403, 75, 18) &&
		Is(archive.pageControlsInvalidation, 211, 403, 450, 18),
		"archive page controls use one drawing and invalidation layout");

	Require(Is(archive.popup.facePanel, 212, 145, 58, 52) &&
		Is(archive.popup.name, 280, 165) &&
		Is(archive.popup.description.origin, 214, 202) &&
		archive.popup.description.width == 296 &&
		Is(archive.popup.section(0), 206, 139) &&
		Is(archive.popup.shadow(0), 210, 143, 309, 8),
		"archive popup artwork and text derive from one popup anchor");
	Require(Is(archive.popup.section(12), 206, 247) &&
		Is(archive.popup.doneButton(11), 472, 231, 36, 16) &&
		Is(archive.popup.doneHitbox(11), 470, 231, 36, 16),
		"archive popup growth keeps the done artwork and hitbox paired");
	constexpr ArchiveLayout shiftedArchive = MakeArchiveLayout(shiftedAnchors);
	Require(Is(shiftedArchive.grid.frame(19), 668, 420, 66, 64) &&
		Is(shiftedArchive.popup.facePanel, 372, 235, 58, 52) &&
		Is(shiftedArchive.popup.doneButton(11), 632, 321, 36, 16),
		"all A.I.M. Archives geometry follows centered-screen translation");

	Require(ArchiveProfileIndex(0, 0, 80) == 0 &&
		ArchiveProfileIndex(3, 19, 80) == 79 &&
		ArchiveProfileIndex(3, 13, 73) == 73 &&
		ArchiveProfileIndex(4, 0, 80) == 80 &&
		ArchiveProfileIndex(0, 20, 80) == 80,
		"archive profile mapping rejects partial-page and exact-end slots");
	bool visibleProfiles[80] = {};
	visibleProfiles[7] = true;
	visibleProfiles[46] = true;
	Require(ArchivePageHasVisible(visibleProfiles, 80, 0) &&
		!ArchivePageHasVisible(visibleProfiles, 80, 1) &&
		ArchivePageHasVisible(visibleProfiles, 80, 2) &&
		!ArchivePageHasVisible(visibleProfiles, 80, 4),
		"archive pages remain discoverable when their first slot is empty");
	const bool enabledPages[kArchivePageCount] = {true, false, true, false};
	const bool noPages[kArchivePageCount] = {};
	Require(NextArchivePage(0, enabledPages) == 2 &&
		NextArchivePage(2, enabledPages) == 0 &&
		NextArchivePage(9, enabledPages) == 2 &&
		NextArchivePage(1, noPages) == 1,
		"archive navigation skips sparse pages and wraps without invalid indices");

	return EXIT_SUCCESS;
}
