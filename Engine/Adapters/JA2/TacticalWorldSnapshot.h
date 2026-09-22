#ifndef ENGINE_ADAPTERS_JA2_TACTICAL_WORLD_SNAPSHOT_H
#define ENGINE_ADAPTERS_JA2_TACTICAL_WORLD_SNAPSHOT_H

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <utility>
#include <vector>

#include <Engine/Adapters/JA2/TacticalEntity.h>

enum class TacticalStance : std::uint8_t
{
	Unknown,
	Standing,
	Crouched,
	Prone
};

// Exact basename passed to JA2's successful LoadWorld call. The fixed storage
// matches gzLastLoadedFile and keeps snapshots/deltas allocation-free. The
// value is an ASCII, NUL-terminated .dat basename: directory separators,
// drive prefixes, controls, and non-canonical padding are rejected so a
// passive client can never turn authority data into an arbitrary asset path.
inline constexpr std::size_t TacticalMapAssetKeyStorageBytes = 260;
inline constexpr std::size_t MaximumTacticalMapAssetKeyBytes =
	TacticalMapAssetKeyStorageBytes - 1;

struct TacticalMapAssetKey
{
	std::array<char, TacticalMapAssetKeyStorageBytes> bytes{};

	bool empty() const noexcept
	{
		for (const char byte : bytes)
			if (byte != '\0') return false;
		return true;
	}
	const char* c_str() const noexcept { return bytes.data(); }

	bool operator==(const TacticalMapAssetKey& other) const noexcept
	{
		return bytes == other.bytes;
	}

	bool operator!=(const TacticalMapAssetKey& other) const noexcept
	{
		return !(*this == other);
	}
};

inline bool IsValidTacticalMapAssetKey(
	const TacticalMapAssetKey& key) noexcept
{
	std::size_t length = 0;
	while (length < key.bytes.size() && key.bytes[length] != '\0')
	{
		const unsigned char byte =
			static_cast<unsigned char>(key.bytes[length]);
		const bool alphaNumeric =
			(byte >= 'A' && byte <= 'Z') ||
			(byte >= 'a' && byte <= 'z') ||
			(byte >= '0' && byte <= '9');
		if (!alphaNumeric && byte != '_' && byte != '-' && byte != '.')
			return false;
		++length;
	}
	if (length == 0 || length > MaximumTacticalMapAssetKeyBytes ||
		length == key.bytes.size())
		return false;
	for (std::size_t index = length; index < key.bytes.size(); ++index)
		if (key.bytes[index] != '\0') return false;
	if (length < 5) return false;
	auto lowerAscii = [](char value) noexcept {
		return value >= 'A' && value <= 'Z'
			? static_cast<char>(value - 'A' + 'a')
			: value;
	};
	return key.bytes[length - 4] == '.' &&
		lowerAscii(key.bytes[length - 3]) == 'd' &&
		lowerAscii(key.bytes[length - 2]) == 'a' &&
		lowerAscii(key.bytes[length - 1]) == 't';
}

// Transactional bounded conversion for legacy fixed buffers and literals.
// Failure leaves output untouched.
inline bool AssignTacticalMapAssetKey(TacticalMapAssetKey& output,
	const char* source, std::size_t availableBytes) noexcept
{
	if (source == nullptr || availableBytes == 0) return false;
	TacticalMapAssetKey candidate;
	std::size_t length = 0;
	while (length < availableBytes &&
		length < TacticalMapAssetKeyStorageBytes && source[length] != '\0')
		++length;
	if (length == availableBytes ||
		length == TacticalMapAssetKeyStorageBytes)
		return false;
	for (std::size_t index = 0; index < length; ++index)
		candidate.bytes[index] = source[index];
	if (!IsValidTacticalMapAssetKey(candidate)) return false;
	output = candidate;
	return true;
}

template <std::size_t Size>
inline bool AssignTacticalMapAssetKey(TacticalMapAssetKey& output,
	const char (&source)[Size]) noexcept
{
	return AssignTacticalMapAssetKey(output, source, Size);
}

struct TacticalSectorSnapshot
{
	std::int16_t x = 0;
	std::int16_t y = 0;
	std::int8_t z = -1;
	bool loaded = false;
	TacticalMapAssetKey mapAssetKey;
};

inline bool IsValidTacticalSectorSnapshot(const TacticalSectorSnapshot& sector,
	bool allowLoadedWithoutMapAssetKey = false) noexcept
{
	if (!sector.loaded) return sector.mapAssetKey.empty();
	return IsValidTacticalMapAssetKey(sector.mapAssetKey) ||
		(allowLoadedWithoutMapAssetKey && sector.mapAssetKey.empty());
}

enum class TacticalInterruptPhase : std::uint8_t
{
	None = 0,
	Resolving = 1,
	Active = 2
};

struct TacticalTurnSnapshot
{
	bool turnBased = false;
	bool inCombat = false;
	std::uint8_t activeTeam = 0;
	std::uint64_t serial = 0;
	// Canonical public projection of the authority's input-busy predicate. Raw
	// pending-action counts and interrupt kinds remain private to the live host.
	bool commandsBlocked = false;
	TacticalInterruptPhase interruptPhase = TacticalInterruptPhase::None;
	std::uint64_t interruptSerial = 0;
};

// Natural world shade applied by the authority. JA2's lighting domain is
// inverted (1 is brightest, 15 is darkest); zero is never a valid scene
// ambient and remains useful for rejecting uninitialized wire data.
struct TacticalWorldLightingSnapshot
{
	static constexpr std::uint8_t Brightest = 1;
	static constexpr std::uint8_t Darkest = 15;
	static constexpr std::uint8_t Default = 4;

	std::uint8_t ambientLightLevel = Default;

	bool valid() const noexcept
	{
		return ambientLightLevel >= Brightest &&
			ambientLightLevel <= Darkest;
	}

	bool operator==(const TacticalWorldLightingSnapshot& other) const noexcept
	{
		return ambientLightLevel == other.ambientLightLevel;
	}

	bool operator!=(const TacticalWorldLightingSnapshot& other) const noexcept
	{
		return !(*this == other);
	}
};

// Exact logical tile extent of one loaded tactical world. These dimensions are
// authority data: passive clients must not consult their cold local JA2 world
// globals to interpret replicated grid numbers. JA2's supported enlarged-map
// ceiling is part of this adapter contract and keeps rows * columns in int32.
struct TacticalWorldDimensions
{
	static constexpr std::uint16_t MaximumColumns = 2000;
	static constexpr std::uint16_t MaximumRows = 2000;

	std::uint16_t columns = 0;
	std::uint16_t rows = 0;

	bool valid() const noexcept
	{
		return columns != 0 && rows != 0 &&
			columns <= MaximumColumns && rows <= MaximumRows;
	}

	bool contains(std::int32_t grid) const noexcept
	{
		return valid() && grid >= 0 &&
			static_cast<std::uint32_t>(grid) <
				static_cast<std::uint32_t>(columns) * rows;
	}
};

// Bounded public projection of the first object in one fixed tactical
// combat-equipment slot (helmet, vest, legs, or either hand).
// ammunitionState distinguishes an ammo-bearing object with no loaded rounds
// from an ordinary item whose union-backed ammunition fields are meaningless.
struct TacticalHandItemSnapshot
{
	std::uint16_t item = 0;
	std::uint8_t quantity = 0;
	std::int16_t condition = 0;
	std::uint16_t ammunitionItem = 0;
	std::uint16_t ammunitionCount = 0;
	std::int16_t ammunitionCondition = 0;
	bool ammunitionState = false;
	bool chambered = false;

	bool valid() const noexcept
	{
		if (item == 0)
			return quantity == 0 && condition == 0 &&
				ammunitionItem == 0 && ammunitionCount == 0 &&
				ammunitionCondition == 0 && !ammunitionState && !chambered;
		if (quantity == 0) return false;
		return ammunitionState ||
			(ammunitionItem == 0 && ammunitionCount == 0 &&
			 ammunitionCondition == 0 && !chambered);
	}

	bool operator==(const TacticalHandItemSnapshot& other) const noexcept
	{
		return item == other.item && quantity == other.quantity &&
			condition == other.condition &&
			ammunitionItem == other.ammunitionItem &&
			ammunitionCount == other.ammunitionCount &&
			ammunitionCondition == other.ammunitionCondition &&
			ammunitionState == other.ammunitionState &&
			chambered == other.chambered;
	}

	bool operator!=(const TacticalHandItemSnapshot& other) const noexcept
	{
		return !(*this == other);
	}
};

struct TacticalActorLoadoutSnapshot
{
	TacticalHandItemSnapshot helmet;
	TacticalHandItemSnapshot vest;
	TacticalHandItemSnapshot legs;
	TacticalHandItemSnapshot primaryHand;
	TacticalHandItemSnapshot secondaryHand;

	bool valid() const noexcept
	{
		return helmet.valid() && vest.valid() && legs.valid() &&
			primaryHand.valid() && secondaryHand.valid();
	}

	bool operator==(const TacticalActorLoadoutSnapshot& other) const noexcept
	{
		return helmet == other.helmet && vest == other.vest &&
			legs == other.legs && primaryHand == other.primaryHand &&
			secondaryHand == other.secondaryHand;
	}

	bool operator!=(const TacticalActorLoadoutSnapshot& other) const noexcept
	{
		return !(*this == other);
	}
};

// Pointer-free renderer inputs. Fixed-point world coordinates avoid copying
// ABI-dependent FLOAT bytes while retaining sub-tile motion. Palette IDs are
// represented by indices into the installed, manifest-covered replacement
// table; presence bits keep index 255 available and make absent fields
// canonical. The animation-height flag preserves the renderer's exact `> 0`
// branch without transporting an otherwise unused float.
inline constexpr std::int32_t TacticalWorldCoordinateScale = 256;
inline constexpr std::int32_t TacticalWorldCellSize = 10;
inline constexpr std::uint8_t TacticalActorBodyTypeCount = 29;
inline constexpr std::uint16_t TacticalAnimationStateCount = 370;
inline constexpr std::uint16_t TacticalAnimationSurfaceCount = 496;
inline constexpr std::uint16_t TacticalAnimationSurfaceAbsent = 32000;
inline constexpr std::size_t TacticalActorDisplayNameCodeUnits = 10;

enum TacticalActorPresentationFlag : std::uint8_t
{
	TacticalActorRenderPosePresent = 1u << 0,
	TacticalActorPositiveAnimationHeight = 1u << 1,
	TacticalActorHeadPalettePresent = 1u << 2,
	TacticalActorPantsPalettePresent = 1u << 3,
	TacticalActorVestPalettePresent = 1u << 4,
	TacticalActorSkinPalettePresent = 1u << 5,
	TacticalActorMultiTileNonZ = 1u << 6,
	TacticalActorMultiTileZ = 1u << 7
};

enum class TacticalPortraitFamily : std::uint8_t
{
	Absent = 0,
	Faces = 1,
	ImpFaces = 2
};

enum class TacticalPortraitCamouflage : std::uint8_t
{
	None = 0,
	Wood = 1,
	Urban = 2,
	Desert = 3,
	Snow = 4
};

// Authority-selected manifest-bound asset descriptor, never a live face handle.
struct TacticalPortraitSnapshot
{
	TacticalPortraitFamily family = TacticalPortraitFamily::Absent;
	std::uint8_t faceIndex = 0;
	TacticalPortraitCamouflage camouflage = TacticalPortraitCamouflage::None;

	bool operator==(const TacticalPortraitSnapshot& other) const noexcept
	{
		return family == other.family && faceIndex == other.faceIndex &&
			camouflage == other.camouflage;
	}
	bool operator!=(const TacticalPortraitSnapshot& other) const noexcept
	{
		return !(*this == other);
	}
};

inline bool IsCanonicalTacticalPortrait(const TacticalPortraitSnapshot& portrait) noexcept
{
	if (static_cast<std::uint8_t>(portrait.family) >
		static_cast<std::uint8_t>(TacticalPortraitFamily::ImpFaces) ||
		static_cast<std::uint8_t>(portrait.camouflage) >
		static_cast<std::uint8_t>(TacticalPortraitCamouflage::Snow)) return false;
	return portrait.family != TacticalPortraitFamily::Absent ||
		(portrait.faceIndex == 0 && portrait.camouflage == TacticalPortraitCamouflage::None);
}

struct TacticalActorPresentationSnapshot
{
	std::uint8_t bodyType = 0;
	std::uint8_t flags = 0;
	std::int8_t animationDirection = 0;
	std::int32_t worldXQ8 = 0;
	std::int32_t worldYQ8 = 0;
	std::int16_t heightAdjustment = 0;
	std::uint16_t animationSurface = TacticalAnimationSurfaceAbsent;
	std::uint16_t animationFrame = 0;
	std::uint8_t headPaletteIndex = 0;
	std::uint8_t pantsPaletteIndex = 0;
	std::uint8_t vestPaletteIndex = 0;
	std::uint8_t skinPaletteIndex = 0;
	std::array<std::uint16_t, TacticalActorDisplayNameCodeUnits>
		displayNameUtf16{};
	TacticalPortraitSnapshot portrait;

	bool operator==(
		const TacticalActorPresentationSnapshot& other) const noexcept
	{
		return bodyType == other.bodyType && flags == other.flags &&
			animationDirection == other.animationDirection &&
			worldXQ8 == other.worldXQ8 && worldYQ8 == other.worldYQ8 &&
			heightAdjustment == other.heightAdjustment &&
			animationSurface == other.animationSurface &&
			animationFrame == other.animationFrame &&
			headPaletteIndex == other.headPaletteIndex &&
			pantsPaletteIndex == other.pantsPaletteIndex &&
			vestPaletteIndex == other.vestPaletteIndex &&
			skinPaletteIndex == other.skinPaletteIndex &&
			displayNameUtf16 == other.displayNameUtf16 && portrait == other.portrait;
	}

	bool operator!=(
		const TacticalActorPresentationSnapshot& other) const noexcept
	{
		return !(*this == other);
	}
};

inline bool IsCanonicalTacticalDisplayName(
	const std::array<std::uint16_t,
		TacticalActorDisplayNameCodeUnits>& name) noexcept
{
	bool terminated = false;
	for (std::size_t index = 0; index < name.size(); ++index)
	{
		const std::uint16_t codeUnit = name[index];
		if (terminated)
		{
			if (codeUnit != 0) return false;
			continue;
		}
		if (codeUnit == 0)
		{
			terminated = true;
			continue;
		}
		if (codeUnit >= 0xd800u && codeUnit <= 0xdbffu)
		{
			if (++index >= name.size()) return false;
			const std::uint16_t low = name[index];
			if (low < 0xdc00u || low > 0xdfffu) return false;
			continue;
		}
		if (codeUnit >= 0xdc00u && codeUnit <= 0xdfffu) return false;
	}
	return terminated;
}

inline bool IsCanonicalTacticalActorPresentation(
	const TacticalActorPresentationSnapshot& presentation) noexcept
{
	if (presentation.bodyType >= TacticalActorBodyTypeCount ||
		!IsCanonicalTacticalDisplayName(presentation.displayNameUtf16) ||
		!IsCanonicalTacticalPortrait(presentation.portrait))
		return false;

	const auto flag = [&](TacticalActorPresentationFlag value) noexcept {
		return (presentation.flags & static_cast<std::uint8_t>(value)) != 0;
	};
	if (!flag(TacticalActorHeadPalettePresent) &&
		presentation.headPaletteIndex != 0)
		return false;
	if (!flag(TacticalActorPantsPalettePresent) &&
		presentation.pantsPaletteIndex != 0)
		return false;
	if (!flag(TacticalActorVestPalettePresent) &&
		presentation.vestPaletteIndex != 0)
		return false;
	if (!flag(TacticalActorSkinPalettePresent) &&
		presentation.skinPaletteIndex != 0)
		return false;

	if (!flag(TacticalActorRenderPosePresent))
	{
		return presentation.animationDirection == 0 &&
			presentation.worldXQ8 == 0 &&
			presentation.worldYQ8 == 0 &&
			presentation.heightAdjustment == 0 &&
			presentation.animationSurface == TacticalAnimationSurfaceAbsent &&
			presentation.animationFrame == 0 &&
			!flag(TacticalActorPositiveAnimationHeight) &&
			!flag(TacticalActorMultiTileNonZ) &&
			!flag(TacticalActorMultiTileZ);
	}

	return presentation.animationDirection >= 0 &&
		presentation.animationDirection < 8 &&
		presentation.worldXQ8 >= 0 && presentation.worldYQ8 >= 0 &&
		presentation.animationSurface < TacticalAnimationSurfaceCount;
}

// Pointer-free view of one TacticalActor. Numeric team/profile/animation values
// remain adapter data so Core and package code never depend on legacy headers.
struct TacticalActorSnapshot
{
	TacticalEntityId id;
	std::uint8_t team = 0;
	std::uint16_t profile = 0;
	std::int32_t grid = -1;
	std::int8_t level = 0;
	std::uint8_t direction = 0;
	std::uint16_t animation = 0;
	TacticalStance stance = TacticalStance::Unknown;
	std::int16_t actionPoints = 0;
	std::int16_t life = 0;
	std::int16_t maximumLife = 0;
	std::int16_t breath = 0;
	std::int16_t maximumBreath = 0;
	bool active = false;
	bool inSector = false;
	// Canonical JA2 authority predicate (OK_ENEMY_MERC), projected so passive
	// clients never infer hostility from numeric team membership.
	bool hostileToPlayerTeam = false;
	bool interruptActionEligible = false;
	TacticalActorLoadoutSnapshot loadout;
	TacticalActorPresentationSnapshot presentation;

	bool operator==(const TacticalActorSnapshot& other) const noexcept
	{
		return id == other.id && team == other.team &&
			profile == other.profile && grid == other.grid &&
			level == other.level && direction == other.direction &&
			animation == other.animation && stance == other.stance &&
			actionPoints == other.actionPoints && life == other.life &&
			maximumLife == other.maximumLife && breath == other.breath &&
			maximumBreath == other.maximumBreath && active == other.active &&
			inSector == other.inSector &&
			hostileToPlayerTeam == other.hostileToPlayerTeam &&
			interruptActionEligible == other.interruptActionEligible &&
			loadout == other.loadout && presentation == other.presentation;
	}

	bool operator!=(const TacticalActorSnapshot& other) const noexcept
	{
		return !(*this == other);
	}
};

inline bool IsValidTacticalActorPresentation(
	const TacticalActorSnapshot& actor,
	const TacticalWorldDimensions& dimensions) noexcept
{
	const TacticalActorPresentationSnapshot& presentation = actor.presentation;
	if (!IsCanonicalTacticalActorPresentation(presentation) ||
		actor.animation >= TacticalAnimationStateCount)
		return false;
	if ((presentation.flags & TacticalActorRenderPosePresent) == 0)
		return true;
	if (!actor.active || !actor.inSector ||
		!dimensions.contains(actor.grid) || actor.level < 0 || actor.level > 1 ||
		actor.direction >= 8)
		return false;
	const std::int64_t maximumX = static_cast<std::int64_t>(dimensions.columns) *
		TacticalWorldCellSize * TacticalWorldCoordinateScale;
	const std::int64_t maximumY = static_cast<std::int64_t>(dimensions.rows) *
		TacticalWorldCellSize * TacticalWorldCoordinateScale;
	if (presentation.worldXQ8 >= maximumX ||
		presentation.worldYQ8 >= maximumY)
		return false;
	// JA2 locomotion advances the logical grid to the next path tile before the
	// sub-tile render position has crossed that tile boundary. Permit that one
	// cardinal/diagonal step while rejecting unrelated render coordinates.
	const std::int32_t encodedCellSize =
		TacticalWorldCellSize * TacticalWorldCoordinateScale;
	const std::int32_t logicalColumn = actor.grid % dimensions.columns;
	const std::int32_t logicalRow = actor.grid / dimensions.columns;
	const std::int32_t renderColumn =
		presentation.worldXQ8 / encodedCellSize;
	const std::int32_t renderRow =
		presentation.worldYQ8 / encodedCellSize;
	const std::int32_t columnDistance = logicalColumn > renderColumn
		? logicalColumn - renderColumn : renderColumn - logicalColumn;
	const std::int32_t rowDistance = logicalRow > renderRow
		? logicalRow - renderRow : renderRow - logicalRow;
	return columnDistance <= 1 && rowDistance <= 1;
}

inline bool IsValidTacticalInterruptPhase(TacticalInterruptPhase phase) noexcept
{
	return phase == TacticalInterruptPhase::None ||
		phase == TacticalInterruptPhase::Resolving ||
		phase == TacticalInterruptPhase::Active;
}

inline bool IsValidTacticalInterruptState(
	const TacticalTurnSnapshot& turn) noexcept
{
	return IsValidTacticalInterruptPhase(turn.interruptPhase) &&
		(turn.interruptPhase == TacticalInterruptPhase::None ||
		 (turn.turnBased && turn.inCombat &&
		  (turn.interruptPhase != TacticalInterruptPhase::Active ||
		   turn.interruptSerial != 0)));
}

inline bool IsValidTacticalInterruptEligibility(
	const TacticalActorSnapshot& actor,
	const TacticalTurnSnapshot& turn) noexcept
{
	return !actor.interruptActionEligible ||
		(turn.interruptPhase == TacticalInterruptPhase::Active && actor.active &&
		 actor.inSector && actor.team == turn.activeTeam);
}

// Public door state deliberately excludes lock, key, trap, perceived-state,
// and structure-database details. baseGrid is the logical identity for the
// current world generation; structureId is an ephemeral optimistic token that
// changes when JA2 swaps the open/closed partner structure.
struct TacticalDoorSnapshot
{
	std::int32_t baseGrid = -1;
	std::uint16_t structureId = 0;
	bool open = false;
};

enum class TacticalSnapshotCreateError
{
	None,
	InvalidEpoch,
	InvalidDimensions,
	InvalidSector,
	InvalidTurn,
	InvalidLighting,
	TooManyActors,
	InvalidEntity,
	DuplicateEntity,
	UnorderedEntity,
	TooManyDoors,
	InvalidDoor,
	DuplicateDoor,
	UnorderedDoor
};

// Immutable, generation-stamped tactical state for packages, diagnostics, and
// deterministic diffing. Construction sorts by stable entity identity and is
// transactional: rejected input leaves the caller's previous snapshot intact.
class TacticalWorldSnapshot
{
public:
	static constexpr std::size_t DefaultMaximumActors = 4096;
	static constexpr std::size_t DefaultMaximumDoors = 1024;

	static TacticalSnapshotCreateError create(
		std::uint64_t epoch,
		TacticalWorldDimensions dimensions,
		TacticalSectorSnapshot sector,
		TacticalTurnSnapshot turn,
		std::vector<TacticalActorSnapshot> actors,
		std::vector<TacticalDoorSnapshot> doors,
		TacticalWorldSnapshot& output,
		std::size_t maximumActors = DefaultMaximumActors,
		std::size_t maximumDoors = DefaultMaximumDoors,
		TacticalWorldLightingSnapshot lighting = {})
	{
		if (epoch == 0) return TacticalSnapshotCreateError::InvalidEpoch;
		if (!dimensions.valid())
			return TacticalSnapshotCreateError::InvalidDimensions;
		if (!IsValidTacticalSectorSnapshot(sector, true))
			return TacticalSnapshotCreateError::InvalidSector;
		if (!IsValidTacticalInterruptState(turn))
			return TacticalSnapshotCreateError::InvalidTurn;
		if (!lighting.valid())
			return TacticalSnapshotCreateError::InvalidLighting;
		if (actors.size() > maximumActors) return TacticalSnapshotCreateError::TooManyActors;
		if (doors.size() > maximumDoors) return TacticalSnapshotCreateError::TooManyDoors;
		for (const TacticalActorSnapshot& actor : actors)
			if (!actor.id.valid() || !actor.loadout.valid() ||
				!IsValidTacticalActorPresentation(actor, dimensions) ||
				!IsValidTacticalInterruptEligibility(actor, turn))
				return TacticalSnapshotCreateError::InvalidEntity;
		for (const TacticalDoorSnapshot& door : doors)
			if (!dimensions.contains(door.baseGrid) || door.structureId == 0)
				return TacticalSnapshotCreateError::InvalidDoor;

		std::sort(actors.begin(), actors.end(),
			[](const TacticalActorSnapshot& left, const TacticalActorSnapshot& right) {
				return left.id < right.id;
			});
		for (std::size_t index = 1; index < actors.size(); ++index)
			if (actors[index - 1].id.slot == actors[index].id.slot)
				return TacticalSnapshotCreateError::DuplicateEntity;
		std::sort(doors.begin(), doors.end(),
			[](const TacticalDoorSnapshot& left, const TacticalDoorSnapshot& right) {
				return left.baseGrid < right.baseGrid;
			});
		for (std::size_t index = 1; index < doors.size(); ++index)
			if (doors[index - 1].baseGrid == doors[index].baseGrid)
				return TacticalSnapshotCreateError::DuplicateDoor;

		TacticalWorldSnapshot accepted;
		accepted.epoch_ = epoch;
		accepted.dimensions_ = dimensions;
		accepted.sector_ = sector;
		accepted.turn_ = turn;
		accepted.lighting_ = lighting;
		accepted.actors_ = std::move(actors);
		accepted.doors_ = std::move(doors);
		output = std::move(accepted);
		return TacticalSnapshotCreateError::None;
	}

	static TacticalSnapshotCreateError create(
		std::uint64_t epoch,
		TacticalWorldDimensions dimensions,
		TacticalSectorSnapshot sector,
		TacticalTurnSnapshot turn,
		std::vector<TacticalActorSnapshot> actors,
		TacticalWorldSnapshot& output,
		std::size_t maximumActors = DefaultMaximumActors,
		TacticalWorldLightingSnapshot lighting = {})
	{
		return create(epoch, dimensions, sector, turn, std::move(actors), {},
			output, maximumActors, DefaultMaximumDoors, lighting);
	}

	// Capture adapters can retain their collection scratch and let the output
	// retain its actor allocation. Validation and sorting finish before output is
	// touched; reserve is the only throwing output operation and has the strong
	// exception guarantee. A successful first call reserves the configured actor
	// ceiling, so later captures within that ceiling require no heap allocation.
	static TacticalSnapshotCreateError createReusable(
		std::uint64_t epoch,
		TacticalWorldDimensions dimensions,
		TacticalSectorSnapshot sector,
		TacticalTurnSnapshot turn,
		std::vector<TacticalActorSnapshot>& actorScratch,
		std::vector<TacticalDoorSnapshot>& doorScratch,
		TacticalWorldSnapshot& output,
		std::size_t maximumActors = DefaultMaximumActors,
		std::size_t maximumDoors = DefaultMaximumDoors,
		TacticalWorldLightingSnapshot lighting = {})
	{
		if (epoch == 0) return TacticalSnapshotCreateError::InvalidEpoch;
		if (!dimensions.valid())
			return TacticalSnapshotCreateError::InvalidDimensions;
		if (!IsValidTacticalSectorSnapshot(sector, true))
			return TacticalSnapshotCreateError::InvalidSector;
		if (!IsValidTacticalInterruptState(turn))
			return TacticalSnapshotCreateError::InvalidTurn;
		if (!lighting.valid())
			return TacticalSnapshotCreateError::InvalidLighting;
		if (actorScratch.size() > maximumActors)
			return TacticalSnapshotCreateError::TooManyActors;
		if (doorScratch.size() > maximumDoors)
			return TacticalSnapshotCreateError::TooManyDoors;
		for (const TacticalActorSnapshot& actor : actorScratch)
			if (!actor.id.valid() || !actor.loadout.valid() ||
				!IsValidTacticalActorPresentation(actor, dimensions) ||
				!IsValidTacticalInterruptEligibility(actor, turn))
				return TacticalSnapshotCreateError::InvalidEntity;
		for (const TacticalDoorSnapshot& door : doorScratch)
			if (!dimensions.contains(door.baseGrid) || door.structureId == 0)
				return TacticalSnapshotCreateError::InvalidDoor;

		std::sort(actorScratch.begin(), actorScratch.end(),
			[](const TacticalActorSnapshot& left, const TacticalActorSnapshot& right) {
				return left.id < right.id;
			});
		for (std::size_t index = 1; index < actorScratch.size(); ++index)
			if (actorScratch[index - 1].id.slot == actorScratch[index].id.slot)
				return TacticalSnapshotCreateError::DuplicateEntity;
		std::sort(doorScratch.begin(), doorScratch.end(),
			[](const TacticalDoorSnapshot& left, const TacticalDoorSnapshot& right) {
				return left.baseGrid < right.baseGrid;
			});
		for (std::size_t index = 1; index < doorScratch.size(); ++index)
			if (doorScratch[index - 1].baseGrid == doorScratch[index].baseGrid)
				return TacticalSnapshotCreateError::DuplicateDoor;

		static_assert(std::is_nothrow_copy_constructible<TacticalActorSnapshot>::value,
			"reusable tactical capture requires non-throwing actor copies");
		static_assert(std::is_nothrow_copy_constructible<TacticalDoorSnapshot>::value,
			"reusable tactical capture requires non-throwing door copies");
		output.actors_.reserve(maximumActors);
		output.doors_.reserve(maximumDoors);
		output.actors_.clear();
		output.doors_.clear();
		for (const TacticalActorSnapshot& actor : actorScratch)
			output.actors_.push_back(actor);
		for (const TacticalDoorSnapshot& door : doorScratch)
			output.doors_.push_back(door);
		output.epoch_ = epoch;
		output.dimensions_ = dimensions;
		output.sector_ = sector;
		output.turn_ = turn;
		output.lighting_ = lighting;
		return TacticalSnapshotCreateError::None;
	}

	static TacticalSnapshotCreateError createReusable(
		std::uint64_t epoch,
		TacticalWorldDimensions dimensions,
		TacticalSectorSnapshot sector,
		TacticalTurnSnapshot turn,
		std::vector<TacticalActorSnapshot>& actorScratch,
		TacticalWorldSnapshot& output,
		std::size_t maximumActors = DefaultMaximumActors,
		TacticalWorldLightingSnapshot lighting = {})
	{
		std::vector<TacticalDoorSnapshot> doors;
		return createReusable(epoch, dimensions, sector, turn, actorScratch,
			doors, output, maximumActors, DefaultMaximumDoors, lighting);
	}

	// JA2's live slot scan already produces strict TacticalEntityId order. This
	// path validates that contract, then exchanges the caller's completed scratch
	// buffer with the output instead of sorting and copying every actor. The
	// caller's scratch is empty after success and retains the output's previous
	// allocation for the next capture.
	static TacticalSnapshotCreateError createReusableOrdered(
		std::uint64_t epoch,
		TacticalWorldDimensions dimensions,
		TacticalSectorSnapshot sector,
		TacticalTurnSnapshot turn,
		std::vector<TacticalActorSnapshot>& actorScratch,
		std::vector<TacticalDoorSnapshot>& doorScratch,
		TacticalWorldSnapshot& output,
		std::size_t maximumActors = DefaultMaximumActors,
		std::size_t maximumDoors = DefaultMaximumDoors,
		TacticalWorldLightingSnapshot lighting = {})
	{
		if (epoch == 0) return TacticalSnapshotCreateError::InvalidEpoch;
		if (!dimensions.valid())
			return TacticalSnapshotCreateError::InvalidDimensions;
		if (!IsValidTacticalSectorSnapshot(sector, true))
			return TacticalSnapshotCreateError::InvalidSector;
		if (!IsValidTacticalInterruptState(turn))
			return TacticalSnapshotCreateError::InvalidTurn;
		if (!lighting.valid())
			return TacticalSnapshotCreateError::InvalidLighting;
		if (actorScratch.size() > maximumActors)
			return TacticalSnapshotCreateError::TooManyActors;
		if (doorScratch.size() > maximumDoors)
			return TacticalSnapshotCreateError::TooManyDoors;
		for (std::size_t index = 0; index < actorScratch.size(); ++index)
		{
			const TacticalActorSnapshot& actor = actorScratch[index];
			if (!actor.id.valid() || !actor.loadout.valid() ||
				!IsValidTacticalActorPresentation(actor, dimensions) ||
				!IsValidTacticalInterruptEligibility(actor, turn))
				return TacticalSnapshotCreateError::InvalidEntity;
			if (index == 0) continue;
			const TacticalEntityId previous = actorScratch[index - 1].id;
			if (previous.slot == actor.id.slot)
				return TacticalSnapshotCreateError::DuplicateEntity;
			if (!(previous < actor.id))
				return TacticalSnapshotCreateError::UnorderedEntity;
		}
		for (std::size_t index = 0; index < doorScratch.size(); ++index)
		{
			const TacticalDoorSnapshot& door = doorScratch[index];
			if (!dimensions.contains(door.baseGrid) || door.structureId == 0)
				return TacticalSnapshotCreateError::InvalidDoor;
			if (index == 0) continue;
			const std::int32_t previous = doorScratch[index - 1].baseGrid;
			if (previous == door.baseGrid)
				return TacticalSnapshotCreateError::DuplicateDoor;
			if (previous > door.baseGrid)
				return TacticalSnapshotCreateError::UnorderedDoor;
		}

		// Reserve before changing observable values so allocation failure keeps the
		// old snapshot intact. Both buffers retain the configured ceiling after the
		// first successful exchange.
		output.actors_.reserve(maximumActors);
		output.doors_.reserve(maximumDoors);
		output.actors_.swap(actorScratch);
		output.doors_.swap(doorScratch);
		actorScratch.clear();
		doorScratch.clear();
		output.epoch_ = epoch;
		output.dimensions_ = dimensions;
		output.sector_ = sector;
		output.turn_ = turn;
		output.lighting_ = lighting;
		return TacticalSnapshotCreateError::None;
	}

	static TacticalSnapshotCreateError createReusableOrdered(
		std::uint64_t epoch,
		TacticalWorldDimensions dimensions,
		TacticalSectorSnapshot sector,
		TacticalTurnSnapshot turn,
		std::vector<TacticalActorSnapshot>& actorScratch,
		TacticalWorldSnapshot& output,
		std::size_t maximumActors = DefaultMaximumActors,
		TacticalWorldLightingSnapshot lighting = {})
	{
		std::vector<TacticalDoorSnapshot> doors;
		return createReusableOrdered(epoch, dimensions, sector, turn,
			actorScratch, doors, output, maximumActors, DefaultMaximumDoors,
			lighting);
	}

	// Copy into caller-owned reusable storage without exposing mutable snapshot
	// internals. Allocation failure leaves all observable output values intact.
	bool copyTo(TacticalWorldSnapshot& output) const noexcept
	{
		if (&output == this) return true;
		try
		{
			static_assert(std::is_nothrow_copy_constructible<TacticalActorSnapshot>::value,
				"reusable tactical copies require non-throwing actor copies");
			static_assert(std::is_nothrow_copy_constructible<TacticalDoorSnapshot>::value,
				"reusable tactical copies require non-throwing door copies");
			output.actors_.reserve(actors_.size());
			output.doors_.reserve(doors_.size());
			output.actors_.clear();
			output.doors_.clear();
			for (const TacticalActorSnapshot& actor : actors_)
				output.actors_.push_back(actor);
			for (const TacticalDoorSnapshot& door : doors_)
				output.doors_.push_back(door);
			output.epoch_ = epoch_;
			output.dimensions_ = dimensions_;
			output.sector_ = sector_;
			output.turn_ = turn_;
			output.lighting_ = lighting_;
			return true;
		}
		catch (...)
		{
			return false;
		}
	}

	std::uint64_t epoch() const { return epoch_; }
	const TacticalWorldDimensions& dimensions() const { return dimensions_; }
	const TacticalSectorSnapshot& sector() const { return sector_; }
	const TacticalTurnSnapshot& turn() const { return turn_; }
	const TacticalWorldLightingSnapshot& lighting() const { return lighting_; }
	const std::vector<TacticalActorSnapshot>& actors() const { return actors_; }
	const std::vector<TacticalDoorSnapshot>& doors() const { return doors_; }

	const TacticalActorSnapshot* find(TacticalEntityId id) const
	{
		const auto actor = std::lower_bound(
			actors_.begin(), actors_.end(), id,
			[](const TacticalActorSnapshot& candidate, TacticalEntityId sought) {
				return candidate.id < sought;
			});
		return actor != actors_.end() && actor->id == id ? &*actor : nullptr;
	}

	const TacticalDoorSnapshot* findDoor(std::int32_t baseGrid) const
	{
		const auto door = std::lower_bound(
			doors_.begin(), doors_.end(), baseGrid,
			[](const TacticalDoorSnapshot& candidate, std::int32_t sought) {
				return candidate.baseGrid < sought;
			});
		return door != doors_.end() && door->baseGrid == baseGrid
			? &*door : nullptr;
	}

private:
	std::uint64_t epoch_ = 0;
	TacticalWorldDimensions dimensions_;
	TacticalSectorSnapshot sector_;
	TacticalTurnSnapshot turn_;
	TacticalWorldLightingSnapshot lighting_;
	std::vector<TacticalActorSnapshot> actors_;
	std::vector<TacticalDoorSnapshot> doors_;
};

#endif
