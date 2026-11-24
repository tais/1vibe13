#ifndef JA2_FULL_ENGINE_COOP_CLIENT_PRESENTATION_PASSIVE_UI_H
#define JA2_FULL_ENGINE_COOP_CLIENT_PRESENTATION_PASSIVE_UI_H

#include "FullEngineCoopClientPresentationCommandFeedback.h"
#include "FullEngineCoopClientPresentationControlFeedback.h"
#include "FullEngineCoopClientPresentationPortraits.h"

#include <Engine/Adapters/JA2/TacticalWorldSnapshot.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

// Renderer-only projection of one authority-published actor.  The passive UI
// owns these values; it never retains a pointer into the replica snapshot.
enum class FullEngineCoopClientPassiveMarkerKind : std::uint8_t
{
	Friendly,
	Assigned,
	Hostile
};

struct FullEngineCoopClientPassiveMarker
{
	TacticalEntityId actor;
	std::int32_t grid = -1;
	std::int8_t level = 0;
	bool hostile = false;
	FullEngineCoopClientPassiveMarkerKind kind =
		FullEngineCoopClientPassiveMarkerKind::Friendly;
	bool selected = false;
	bool targeted = false;

	bool operator==(const FullEngineCoopClientPassiveMarker& other) const noexcept
	{
		return actor == other.actor && grid == other.grid &&
			level == other.level && hostile == other.hostile && kind == other.kind &&
			selected == other.selected && targeted == other.targeted;
	}
};

// Presentation-owned copy of one authority-published door identity.  The
// structure id and state remain paired with the base grid so a local marker
// can never silently follow a swapped open/closed partner structure.
struct FullEngineCoopClientPassiveDoorMarker
{
	std::int32_t baseGrid = -1;
	std::uint16_t structureId = 0;
	bool open = false;
	bool selected = false;

	bool operator==(
		const FullEngineCoopClientPassiveDoorMarker& other) const noexcept
	{
		return baseGrid == other.baseGrid &&
			structureId == other.structureId && open == other.open &&
			selected == other.selected;
	}
};

// The stock TEAM chrome has six tactical slots.  Co-op ownership can be wider,
// so the complete authority-provided roster is retained and rendering selects
// one deterministic six-entry window containing the current selection.
inline constexpr std::size_t
	FullEngineCoopClientPassiveUiRosterVisibleSlots = 6;
// Smallest stock TEAM-panel extent that contains all six slot rectangles.
inline constexpr std::int32_t
	FullEngineCoopClientPassiveUiRosterMinimumPanelWidth = 503;
inline constexpr std::int32_t
	FullEngineCoopClientPassiveUiRosterMinimumPanelHeight = 116;

struct FullEngineCoopClientPassiveRosterEntry
{
	TacticalEntityId actor;
	std::uint8_t team = 0;
	std::uint16_t profile = 0;
	std::uint8_t bodyType = 0;
	TacticalPortraitSnapshot portrait;
	std::array<std::uint16_t, TacticalActorDisplayNameCodeUnits>
		displayNameUtf16{};
	TacticalStance stance = TacticalStance::Unknown;
	std::int16_t actionPoints = 0;
	std::int16_t life = 0;
	std::int16_t maximumLife = 0;
	std::int16_t breath = 0;
	std::int16_t maximumBreath = 0;
	TacticalHandItemSnapshot primaryHand;
	TacticalHandItemSnapshot secondaryHand;
	bool interruptActionEligible = false;
	bool selected = false;

	bool operator==(
		const FullEngineCoopClientPassiveRosterEntry& other) const noexcept
	{
		return actor == other.actor && team == other.team &&
			profile == other.profile && bodyType == other.bodyType &&
			portrait == other.portrait &&
			displayNameUtf16 == other.displayNameUtf16 &&
			stance == other.stance && actionPoints == other.actionPoints &&
			life == other.life && maximumLife == other.maximumLife &&
			breath == other.breath &&
			maximumBreath == other.maximumBreath &&
			primaryHand == other.primaryHand &&
			secondaryHand == other.secondaryHand &&
			interruptActionEligible == other.interruptActionEligible &&
			selected == other.selected;
	}

	bool operator!=(
		const FullEngineCoopClientPassiveRosterEntry& other) const noexcept
	{
		return !(*this == other);
	}
};

struct FullEngineCoopClientPassiveUiModel
{
	std::uint64_t worldGeneration = 0;
	TacticalWorldDimensions dimensions;
	TacticalMapAssetKey mapAssetKey;
	TacticalEntityId selectedActor;
	std::vector<FullEngineCoopClientPassiveMarker> markers;
	std::vector<FullEngineCoopClientPassiveDoorMarker> doors;
	// availableActors is an owned, pointer-free status catalog used to rebuild
	// the peer-specific roster when admission changes without a world revision.
	std::vector<FullEngineCoopClientPassiveRosterEntry> availableActors;
	std::vector<FullEngineCoopClientPassiveRosterEntry> roster;
	std::size_t rosterFirstVisible = 0;
	FullEngineCoopClientPresentationCommandFeedbackTextPlan commandFeedback;
	FullEngineCoopClientPresentationControlFeedbackTextPlan controlFeedback;
};

enum class FullEngineCoopClientPassiveUiModelResult : std::uint8_t
{
	Success,
	InvalidSnapshot,
	InvalidAssignments,
	TooManyMarkers,
	AllocationFailure
};

// Strongly transactional and bounded by the co-op wire ceilings. Only active,
// in-sector actors with a renderer pose become pins or owned roster entries;
// every assignment must resolve to one exact same-team actor.
FullEngineCoopClientPassiveUiModelResult
BuildFullEngineCoopClientPassiveUiModel(
	const TacticalWorldSnapshot& snapshot,
	const TacticalEntityId* assignedActors,
	std::size_t assignedActorCount,
	TacticalEntityId selectedActor,
	FullEngineCoopClientPassiveUiModel& output) noexcept;

struct FullEngineCoopClientPassiveUiRect
{
	std::int32_t left = 0;
	std::int32_t top = 0;
	std::int32_t right = 0;  // exclusive
	std::int32_t bottom = 0; // exclusive

	bool valid() const noexcept
	{
		return left >= 0 && top >= 0 && right > left && bottom > top;
	}
};

struct FullEngineCoopClientPassiveUiLayout
{
	std::int32_t screenWidth = 0;
	std::int32_t screenHeight = 0;
	FullEngineCoopClientPassiveUiRect panel;
	FullEngineCoopClientPassiveUiRect radar;

	bool valid() const noexcept;
};

struct FullEngineCoopClientPassiveUiProjection
{
	FullEngineCoopClientPassiveUiRect radar;
	double scaleX = 0.0;
	double scaleY = 0.0;
	// Absolute isometric screen coordinates for map cell (0, 0).
	std::int32_t markerOriginX = 0;
	std::int32_t markerOriginY = 0;
	// Presentation-owned local camera state, in JA2 world-pixel coordinates.
	std::int32_t cameraCenterX = 0;
	std::int32_t cameraCenterY = 0;
	std::int32_t viewportWidth = 0;
	std::int32_t viewportHeight = 0;

	bool valid() const noexcept;
};

struct FullEngineCoopClientPassiveUiPoint
{
	std::int32_t x = 0;
	std::int32_t y = 0;
};

// Pure helpers used by both production rendering and data-free regression
// tests. Outputs are clipped for each marker's complete raster extent.
bool ProjectFullEngineCoopClientPassiveMarker(
	const FullEngineCoopClientPassiveMarker& marker,
	const TacticalWorldDimensions& dimensions,
	const FullEngineCoopClientPassiveUiProjection& projection,
	FullEngineCoopClientPassiveUiPoint& output) noexcept;

bool ProjectFullEngineCoopClientPassiveDoorMarker(
	const FullEngineCoopClientPassiveDoorMarker& marker,
	const TacticalWorldDimensions& dimensions,
	const FullEngineCoopClientPassiveUiProjection& projection,
	FullEngineCoopClientPassiveUiPoint& output) noexcept;

bool ProjectFullEngineCoopClientPassiveCamera(
	const FullEngineCoopClientPassiveUiProjection& projection,
	FullEngineCoopClientPassiveUiRect& output) noexcept;

enum class FullEngineCoopClientPassiveUiColor : std::uint8_t
{
	Camera,
	Friendly,
	FriendlyRoof,
	Assigned,
	AssignedRoof,
	Hostile,
	HostileRoof,
	Selected,
	SelectedRoof,
	Target,
	TargetRoof,
	DoorClosed,
	DoorOpen,
	DoorSelected,
	Count
};

FullEngineCoopClientPassiveUiColor
FullEngineCoopClientPassiveMarkerColor(
	const FullEngineCoopClientPassiveMarker& marker) noexcept;

inline constexpr std::size_t FullEngineCoopClientPassiveUiAssetPathBytes = 100;

// Returns the stock TEAM chrome selected solely from initialized native layout.
const char* FullEngineCoopClientPassiveTeamPanelAsset(
	const FullEngineCoopClientPassiveUiLayout& layout) noexcept;

// Produces exactly RADARMAPS\<authority basename without .dat>.STI.  The key
// is never interpreted as a path and output is untouched on failure.
bool BuildFullEngineCoopClientPassiveRadarAssetPath(
	const TacticalMapAssetKey& key,
	char* output,
	std::size_t outputCapacity) noexcept;

struct FullEngineCoopClientPassiveUiServices
{
	using LoadResource = bool (*)(const char* path,
		std::uint32_t& handle, void* context) noexcept;
	using ShadeResource = bool (*)(std::uint32_t handle,
		std::uint8_t shade, void* context) noexcept;
	using ReleaseResource = void (*)(std::uint32_t handle,
		void* context) noexcept;
	using BlitResource = bool (*)(std::uint32_t handle,
		std::int32_t x, std::int32_t y, void* context) noexcept;
	using RenderOverlay = bool (*)(
		const FullEngineCoopClientPassiveUiModel& model,
		const FullEngineCoopClientPassiveUiLayout& layout,
		const FullEngineCoopClientPassiveUiProjection& projection,
		void* context) noexcept;

	LoadResource loadPanel = nullptr;
	LoadResource loadRadar = nullptr;
	ShadeResource shadeRadar = nullptr;
	ReleaseResource release = nullptr;
	BlitResource blit = nullptr;
	RenderOverlay renderOverlay = nullptr;
	void* context = nullptr;
	// Optional cosmetic services; older test/service aggregates remain valid.
	FullEngineCoopClientPortraitServices portraits;
};

enum class FullEngineCoopClientPassiveUiResult : std::uint8_t
{
	Success,
	InvalidInput,
	ModelFailure,
	PanelAssetFailure,
	RadarPathFailure,
	RadarAssetFailure,
	RadarShadeFailure
};

enum class FullEngineCoopClientPassiveUiRenderResult : std::uint8_t
{
	NotReady,
	Success,
	PanelBlitFailure,
	RadarBlitFailure,
	OverlayFailure
};

const char* FullEngineCoopClientPassiveUiResultName(
	FullEngineCoopClientPassiveUiResult result) noexcept;
const char* FullEngineCoopClientPassiveUiRenderResultName(
	FullEngineCoopClientPassiveUiRenderResult result) noexcept;

class FullEngineCoopClientPresentationPassiveUi final
{
public:
	FullEngineCoopClientPresentationPassiveUi() noexcept;
	explicit FullEngineCoopClientPresentationPassiveUi(
		FullEngineCoopClientPassiveUiServices services) noexcept;
	~FullEngineCoopClientPresentationPassiveUi();

	FullEngineCoopClientPresentationPassiveUi(
		const FullEngineCoopClientPresentationPassiveUi&) = delete;
	FullEngineCoopClientPresentationPassiveUi& operator=(
		const FullEngineCoopClientPresentationPassiveUi&) = delete;

	FullEngineCoopClientPassiveUiResult synchronize(
		const TacticalWorldSnapshot& snapshot,
		const TacticalEntityId* assignedActors,
		std::size_t assignedActorCount,
		TacticalEntityId selectedActor,
		const FullEngineCoopClientPassiveUiLayout& layout) noexcept;

	void updateSelected(TacticalEntityId selectedActor) noexcept;
	// Assignment membership can change independently of a tactical revision.
	// Reclassifies the owned pins synchronously so a duplicate revision cannot
	// retain authority or selection styling from the previous admission view.
	bool updateAssignments(const TacticalEntityId* assignedActors,
		std::size_t assignedActorCount,
		TacticalEntityId selectedActor) noexcept;
	// Receipts can advance without a tactical snapshot revision. Refresh this
	// pointer-free line at the render boundary so native GAME_SCREEN feedback
	// never waits for an unrelated replica change.
	bool updateCommandFeedback(
		const FullEngineCoopClientPresentationCommandFeedbackInput& input)
		noexcept;
	// Revalidates the synchronized controller mode against the exact snapshot
	// and updates both the native status line and attack-target radar marker.
	// Failure preserves all prior control feedback and marker state.
	bool updateControlFeedback(
		const TacticalWorldSnapshot& snapshot,
		const FullEngineCoopClientPresentationControlFeedbackInput& input)
		noexcept;

	FullEngineCoopClientPassiveUiRenderResult render(
		const FullEngineCoopClientPassiveUiProjection& projection) noexcept;

	void teardown() noexcept;
	bool ready() const noexcept { return ready_; }
	const FullEngineCoopClientPassiveUiModel& model() const noexcept
	{
		return model_;
	}
	const FullEngineCoopClientPassiveUiLayout& layout() const noexcept
	{
		return layout_;
	}
	FullEngineCoopClientPassiveUiRenderResult lastRenderResult() const noexcept
	{
		return renderResult_;
	}

private:
	bool servicesValid() const noexcept;
	void releaseHandles() noexcept;
	void synchronizePortraits() noexcept;

	FullEngineCoopClientPassiveUiServices services_;
	FullEngineCoopClientPresentationPortraits portraits_;
	FullEngineCoopClientPassiveUiModel model_;
	FullEngineCoopClientPassiveUiLayout layout_;
	std::uint32_t panelHandle_ = 0;
	std::uint32_t radarHandle_ = 0;
	FullEngineCoopClientPassiveUiRenderResult renderResult_ =
		FullEngineCoopClientPassiveUiRenderResult::NotReady;
	bool panelLoaded_ = false;
	bool radarLoaded_ = false;
	bool ready_ = false;
};

#endif
