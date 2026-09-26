#include "TacticalEntityHost.h"

#include <cmath>
#include <cstddef>
#include <limits>
#include <optional>

#include <Engine/Adapters/JA2/TacticalEntityRoster.h>

#include "Animation Control.h"
#include "Animation Data.h"
#include "GameSettings.h"
#include "Item Types.h"
#include "Soldier Profile.h"
#include "Soldier Profile Constants.h"
#include "Soldier Palette.h"
#include "SoldierRepository.h"
#include "TacticalActor.h"
#include "TacticalActorStateFlags.h"
#include "Soldier macros.h"
#include "Strategic Movement.h"
#include "StrategicSquadHost.h"
#include "VehiclePassengerHost.h"

namespace
{
static_assert(TOTALBODYTYPES == TacticalActorBodyTypeCount,
	"the tactical presentation wire body-type domain must match JA2");
static_assert(NUMANIMATIONSTATES == TacticalAnimationStateCount,
	"the tactical presentation wire animation-state domain must match JA2");
static_assert(NUMANIMATIONSURFACETYPES == TacticalAnimationSurfaceCount,
	"the tactical presentation wire surface domain must match JA2");
static_assert(INVALID_ANIMATION_SURFACE == TacticalAnimationSurfaceAbsent,
	"the tactical presentation wire absent-surface sentinel must match JA2");
static_assert(NUM_WORLD_DIRECTIONS == 8,
	"the tactical presentation wire animation-direction domain must match JA2");

TacticalEntityDirectory& StandaloneDirectory() noexcept
{
	static TacticalEntityDirectory directory(
		GetJa2SoldierRepository().capacity());
	return directory;
}

TacticalEntityDirectory*& BoundDirectory() noexcept
{
	static TacticalEntityDirectory* directory = &StandaloneDirectory();
	return directory;
}

TacticalEntityRoster& ActiveActorRoster() noexcept
{
	static TacticalEntityRoster roster(
		GetJa2SoldierRepository().capacity());
	return roster;
}

TacticalEntityRoster& AwayActorRoster() noexcept
{
	static TacticalEntityRoster roster(
		GetJa2SoldierRepository().capacity());
	return roster;
}

TacticalEntityId LegacyIdentity(const TacticalActor& soldier) noexcept
{
	return TacticalEntityId{
		static_cast<std::uint16_t>(soldier.identity().id()),
		soldier.identity().incarnation()};
}

TacticalStance LegacyStance(const TacticalActor& soldier) noexcept
{
	if (soldier.animationPlayback().state() >= NUMANIMATIONSTATES)
		return TacticalStance::Unknown;
	switch (gAnimControl[soldier.animationPlayback().state()].ubHeight)
	{
		case ANIM_STAND: return TacticalStance::Standing;
		case ANIM_CROUCH: return TacticalStance::Crouched;
		case ANIM_PRONE: return TacticalStance::Prone;
		default: return TacticalStance::Unknown;
	}
}

bool PaletteIdTerminated(const CHAR8* id) noexcept
{
	if (id == nullptr) return false;
	for (std::size_t index = 0; index < sizeof(PaletteRepID); ++index)
		if (id[index] == '\0') return true;
	return false;
}

bool ValidPaletteReplacementTable() noexcept
{
	if (guiNumReplacements > 256u ||
		(guiNumReplacements != 0 && gpPalRep == nullptr))
		return false;
	for (std::size_t index = 0; index < guiNumReplacements; ++index)
		if (!PaletteIdTerminated(gpPalRep[index].ID)) return false;
	return true;
}

bool EncodePaletteIndex(const PaletteRepID& id,
	TacticalActorPresentationFlag presenceFlag,
	std::uint8_t& flags,
	std::uint8_t& index) noexcept
{
	if (!PaletteIdTerminated(id)) return false;
	if (id[0] == '\0')
	{
		index = 0;
		return true;
	}
	UINT8 resolved = 0;
	if (!GetPaletteRepIndexFromID(id, &resolved)) return false;
	index = resolved;
	flags |= static_cast<std::uint8_t>(presenceFlag);
	return true;
}

void EncodeDisplayName(const SoldierIdentityComponent::Name& source,
	std::array<std::uint16_t,
		TacticalActorDisplayNameCodeUnits>& destination) noexcept
{
	destination.fill(0);
	std::size_t output = 0;
	for (std::size_t input = 0;
		input < SOLDIER_NAME_LENGTH && output + 1 < destination.size(); ++input)
	{
		if (source[input] == L'\0') break;
		std::uint32_t scalar = 0xfffdu;
		if constexpr (sizeof(CHAR16) == 2)
		{
			const std::uint16_t first =
				static_cast<std::uint16_t>(source[input]);
			if (first >= 0xd800u && first <= 0xdbffu &&
				input + 1 < SOLDIER_NAME_LENGTH)
			{
				const std::uint16_t second =
					static_cast<std::uint16_t>(source[input + 1]);
				if (second >= 0xdc00u && second <= 0xdfffu)
				{
					scalar = 0x10000u +
						((static_cast<std::uint32_t>(first) - 0xd800u) << 10) +
						(static_cast<std::uint32_t>(second) - 0xdc00u);
					++input;
				}
			}
			else if (first < 0xd800u || first > 0xdfffu)
			{
				scalar = first;
			}
		}
		else
		{
			const std::uint32_t candidate =
				static_cast<std::uint32_t>(source[input]);
			if (candidate <= 0x10ffffu &&
				(candidate < 0xd800u || candidate > 0xdfffu))
				scalar = candidate;
		}

		if (scalar <= 0xffffu)
		{
			destination[output++] = static_cast<std::uint16_t>(scalar);
		}
		else
		{
			if (output + 2 >= destination.size()) break;
			scalar -= 0x10000u;
			destination[output++] = static_cast<std::uint16_t>(
				0xd800u + (scalar >> 10));
			destination[output++] = static_cast<std::uint16_t>(
				0xdc00u + (scalar & 0x3ffu));
		}
	}
}

bool EncodeWorldCoordinate(FLOAT coordinate,
	std::int32_t& encoded) noexcept
{
	if (!std::isfinite(coordinate) || coordinate < 0.0f) return false;
	const double scaled = std::round(
		static_cast<double>(coordinate) * TacticalWorldCoordinateScale);
	if (scaled < 0.0 ||
		scaled > static_cast<double>(std::numeric_limits<std::int32_t>::max()))
		return false;
	encoded = static_cast<std::int32_t>(scaled);
	return true;
}

TacticalPortraitSnapshot LegacyPortraitState(const TacticalActor& soldier) noexcept
{
	const auto profileId = soldier.identity().profile();
	if (profileId == NO_PROFILE || profileId >= NUM_PROFILES) return {};
	const MERCPROFILESTRUCT& profile = gMercProfiles[profileId];
	TacticalPortraitSnapshot portrait;
	portrait.family = profile.Type == PROFILETYPE_IMP
		? TacticalPortraitFamily::ImpFaces : TacticalPortraitFamily::Faces;
	// InternalInitFace's small-face alias is keyed by PROFILE, not face index.
	portrait.faceIndex = profileId >= 151 && profileId <= 154
		? 151 : profile.ubFaceIndex;
	if (gGameExternalOptions.fShowCamouflageFaces)
	{
		// Mirror SetCamoFace's last-positive APPLIED-camo policy. Its legacy
		// per-profile presentation cache can be stale after loading or wear-off,
		// and headless authority need not allocate any face. Read current actor
		// state directly: worn clothing does not paint the small portrait.
		const auto& camo = soldier.camouflage();
		if (camo.snowApplied() > 0) portrait.camouflage = TacticalPortraitCamouflage::Snow;
		else if (camo.desertApplied() > 0) portrait.camouflage = TacticalPortraitCamouflage::Desert;
		else if (camo.urbanApplied() > 0) portrait.camouflage = TacticalPortraitCamouflage::Urban;
		else if (camo.jungleApplied() > 0) portrait.camouflage = TacticalPortraitCamouflage::Wood;
	}
	return portrait;
}

bool LegacyPresentationState(const TacticalActor& soldier,
	TacticalActorPresentationSnapshot& presentation) noexcept
{
	if (soldier.identity().bodyType() >= TOTALBODYTYPES ||
		soldier.animationPlayback().state() >= NUMANIMATIONSTATES ||
		!ValidPaletteReplacementTable())
		return false;

	TacticalActorPresentationSnapshot candidate;
	candidate.bodyType = soldier.identity().bodyType();
	candidate.portrait = LegacyPortraitState(soldier);
	EncodeDisplayName(soldier.identity().name(), candidate.displayNameUtf16);
	if (!EncodePaletteIndex(soldier.renderState().headPalette(),
			TacticalActorHeadPalettePresent, candidate.flags,
			candidate.headPaletteIndex) ||
		!EncodePaletteIndex(soldier.renderState().pantsPalette(),
			TacticalActorPantsPalettePresent, candidate.flags,
			candidate.pantsPaletteIndex) ||
		!EncodePaletteIndex(soldier.renderState().vestPalette(),
			TacticalActorVestPalettePresent, candidate.flags,
			candidate.vestPaletteIndex) ||
		!EncodePaletteIndex(soldier.renderState().skinPalette(),
			TacticalActorSkinPalettePresent, candidate.flags,
			candidate.skinPaletteIndex))
		return false;

	if (soldier.roster().active() && soldier.roster().inSector() &&
		soldier.position().gridNo() >= 0)
	{
		if (soldier.position().level() < 0 || soldier.position().level() > 1 ||
			soldier.position().direction() >= 8 ||
			soldier.movement().animationDirection() < 0 ||
			soldier.movement().animationDirection() >= NUM_WORLD_DIRECTIONS ||
			soldier.animationPlayback().surface() >=
				NUMANIMATIONSURFACETYPES ||
			!std::isfinite(soldier.position().animationHeightAdjustment()) ||
			!EncodeWorldCoordinate(
				soldier.position().worldX(), candidate.worldXQ8) ||
			!EncodeWorldCoordinate(
				soldier.position().worldY(), candidate.worldYQ8))
			return false;
		candidate.flags |= TacticalActorRenderPosePresent;
		candidate.animationDirection =
			soldier.movement().animationDirection();
		if (soldier.position().animationHeightAdjustment() > 0.0f)
			candidate.flags |= TacticalActorPositiveAnimationHeight;
		if ((soldier.status().flags() & SOLDIER_MULTITILE_NZ) != 0)
			candidate.flags |= TacticalActorMultiTileNonZ;
		if ((soldier.status().flags() & SOLDIER_MULTITILE_Z) != 0)
			candidate.flags |= TacticalActorMultiTileZ;
		candidate.heightAdjustment = soldier.position().heightAdjustment();
		candidate.animationSurface = soldier.animationPlayback().surface();
		candidate.animationFrame = soldier.animationPlayback().frame();
	}

	presentation = candidate;
	return true;
}

std::optional<TacticalHandItemSnapshot> LegacyEquipmentSlotState(
	const OBJECTTYPE& object) noexcept
{
	const bool emptyItem = object.usItem == NOTHING;
	const bool emptyStack = object.ubNumberOfObjects == 0;
	if (emptyItem || emptyStack)
	{
		if (emptyItem && emptyStack) return TacticalHandItemSnapshot{};
		return std::nullopt;
	}
	if (object.usItem >= MAXITEMS || object.objectStack.empty())
		return std::nullopt;

	const StackedObjectData& first = object.objectStack.front();
	TacticalHandItemSnapshot state;
	state.item = object.usItem;
	state.quantity = object.ubNumberOfObjects;
	state.condition = first.data.objectStatus;
	if ((Item[object.usItem].usItemClass & (IC_GUN | IC_LAUNCHER)) != 0)
	{
		const ObjectDataStructs::OBJECT_GUN& gun = first.data.gun;
		if (gun.usGunAmmoItem != NOTHING && gun.usGunAmmoItem >= MAXITEMS)
			return std::nullopt;
		state.ammunitionItem = gun.usGunAmmoItem;
		state.ammunitionCount = gun.ubGunShotsLeft;
		state.ammunitionCondition = gun.bGunAmmoStatus;
		state.ammunitionState = true;
		state.chambered =
			(gun.ubGunState & GS_CARTRIDGE_IN_CHAMBER) != 0;
	}
	return state;
}

std::optional<TacticalActorSnapshot> LegacyState(
	const TacticalActor& soldier) noexcept
{
	const std::optional<TacticalHandItemSnapshot> helmet =
		LegacyEquipmentSlotState(soldier.inventory()[HELMETPOS]);
	const std::optional<TacticalHandItemSnapshot> vest =
		LegacyEquipmentSlotState(soldier.inventory()[VESTPOS]);
	const std::optional<TacticalHandItemSnapshot> legs =
		LegacyEquipmentSlotState(soldier.inventory()[LEGPOS]);
	const std::optional<TacticalHandItemSnapshot> primaryHand =
		LegacyEquipmentSlotState(soldier.inventory()[HANDPOS]);
	const std::optional<TacticalHandItemSnapshot> secondaryHand =
		LegacyEquipmentSlotState(soldier.inventory()[SECONDHANDPOS]);
	if (!helmet || !vest || !legs || !primaryHand || !secondaryHand)
		return std::nullopt;

	TacticalActorSnapshot state{
		LegacyIdentity(soldier),
		static_cast<std::uint8_t>(soldier.roster().team()),
		static_cast<std::uint16_t>(soldier.identity().profile()),
		soldier.position().gridNo(),
		static_cast<std::int8_t>(soldier.position().level()),
		soldier.position().direction(),
		soldier.animationPlayback().state(),
		LegacyStance(soldier),
		soldier.actionPoints().current(),
		soldier.vitals().health(),
		soldier.vitals().maximumHealth(),
		soldier.vitals().breath(),
		soldier.vitals().maximumBreath(),
		soldier.roster().active() != FALSE,
		soldier.roster().inSector() != FALSE,
		OK_ENEMY_MERC((&soldier)) != FALSE};
	state.loadout = TacticalActorLoadoutSnapshot{
		*helmet, *vest, *legs, *primaryHand, *secondaryHand};
	if (!LegacyPresentationState(soldier, state.presentation))
		return std::nullopt;
	return state;
}

void RebindRosterAfterRecordSwap(
	TacticalEntityRoster& roster) noexcept
{
	for (std::size_t slot = 0;
		slot < roster.highWaterMark(); ++slot)
	{
		const TacticalEntityId previous = roster.actor(slot);
		if (!previous.valid()) continue;

		const TacticalEntityId rebound =
			GetJa2TacticalEntityId(previous.slot);
		if (!rebound.valid())
		{
			(void)roster.erase(previous);
			continue;
		}
		if (!roster.replace(slot, rebound))
		{
			// A malformed duplicate must not leave an exact reference pointing
			// at the actor that occupied this repository slot before the swap.
			(void)roster.erase(previous);
		}
	}
}

std::int32_t AddActor(
	TacticalEntityRoster& roster,
	TacticalEntityId actor) noexcept
{
	if (!ResolveJa2TacticalEntity(actor)) return -1;
	const std::optional<TacticalEntityRoster::Slot> slot =
		roster.insert(actor);
	if (!slot ||
		*slot > static_cast<std::size_t>(
			std::numeric_limits<std::int32_t>::max()))
	{
		return -1;
	}
	return static_cast<std::int32_t>(*slot);
}
}

void BindJa2TacticalEntityDirectory(
	TacticalEntityDirectory& directory) noexcept
{
	const std::uint32_t nextIncarnation =
		BoundDirectory()->nextIncarnation();
	BoundDirectory() = &directory;
	directory.restoreNextIncarnation(nextIncarnation);
	RebuildJa2TacticalEntityDirectory();
}

TacticalEntityDirectory& GetJa2TacticalEntityDirectory() noexcept
{
	return *BoundDirectory();
}

std::uint32_t IssueJa2TacticalEntityIncarnation() noexcept
{
	return BoundDirectory()->issueIncarnation();
}

std::uint32_t NextJa2TacticalEntityIncarnation() noexcept
{
	return BoundDirectory()->nextIncarnation();
}

void RestoreJa2TacticalEntityIncarnationSequence(
	std::uint32_t nextIncarnation) noexcept
{
	BoundDirectory()->restoreNextIncarnation(nextIncarnation);
}

bool AdoptJa2TacticalEntity(TacticalActor& soldier) noexcept
{
	const TacticalEntityId entity = LegacyIdentity(soldier);
	if (!entity.valid() ||
		!GetJa2SoldierRepository().contains(entity.slot, soldier) ||
		!soldier.roster().active())
		return false;
	const std::optional<TacticalActorSnapshot> state = LegacyState(soldier);
	if (!state) return false;
	if (!BoundDirectory()->activate(entity)) return false;
	if (BoundDirectory()->publishState(*state)) return true;
	(void)BoundDirectory()->release(entity);
	return false;
}

bool ReleaseJa2TacticalEntity(const TacticalActor& soldier) noexcept
{
	const TacticalEntityId entity = LegacyIdentity(soldier);
	if (!GetJa2SoldierRepository().contains(entity.slot, soldier))
		return false;
	return BoundDirectory()->release(entity);
}

bool SynchronizeJa2TacticalEntityState(
	const TacticalActor& soldier) noexcept
{
	const TacticalEntityId entity = LegacyIdentity(soldier);
	if (!entity.valid() ||
		!GetJa2SoldierRepository().contains(entity.slot, soldier) ||
		!soldier.roster().active() ||
		!BoundDirectory()->contains(entity))
		return false;
	const std::optional<TacticalActorSnapshot> state = LegacyState(soldier);
	return state && BoundDirectory()->publishState(*state);
}

bool SynchronizeJa2TacticalEntityStates() noexcept
{
	Ja2SoldierRepository& soldiers = GetJa2SoldierRepository();
	std::size_t synchronized = 0;
	for (std::size_t slot = 0;
		slot < soldiers.capacity(); ++slot)
	{
		const TacticalActor* soldier = soldiers.resolve(slot);
		if (!soldier || !soldier->roster().active())
		{
			if (BoundDirectory()->identity(slot).valid()) return false;
			continue;
		}
		if (!SynchronizeJa2TacticalEntityState(*soldier)) return false;
		++synchronized;
	}
	return synchronized == BoundDirectory()->activeCount() &&
		synchronized == BoundDirectory()->stateCount();
}

void ResetJa2TacticalEntityDirectory() noexcept
{
	BoundDirectory()->reset();
}

void RebuildJa2TacticalEntityDirectory() noexcept
{
	Ja2SoldierRepository& soldiers = GetJa2SoldierRepository();
	ResetJa2TacticalEntityDirectory();
	for (std::size_t slot = 0;
		slot < soldiers.capacity(); ++slot)
	{
		TacticalActor* soldier = soldiers.resolve(slot);
		if (soldier) (void)AdoptJa2TacticalEntity(*soldier);
	}
}

bool SwapJa2TacticalEntitySlots(
	std::uint16_t firstSlot, std::uint16_t secondSlot)
{
	if (!GetJa2SoldierRepository().swapRecords(
			firstSlot, secondSlot))
		return false;
	RebuildJa2TacticalEntityDirectory();
	// The compatibility pool keeps fixed record addresses. Historically a
	// roster entry therefore followed the identity newly occupying that
	// address after a whole-record swap. Rebind exact IDs by repository slot
	// to preserve that behavior without retaining stale incarnations.
	RebindRosterAfterRecordSwap(ActiveActorRoster());
	RebindRosterAfterRecordSwap(AwayActorRoster());
	RebindJa2StrategicSquadRostersAfterRecordSwap();
	RebindJa2VehicleOccupantsAfterRecordSwap();
	RebindStrategicGroupMembersAfterRecordSwap();
	return true;
}

TacticalActor* ResolveJa2TacticalEntity(TacticalEntityId entity) noexcept
{
	if (!BoundDirectory()->contains(entity))
		return nullptr;
	TacticalActor* soldier =
		GetJa2SoldierRepository().resolve(entity.slot);
	if (!soldier || !soldier->roster().active() ||
		static_cast<std::uint16_t>(soldier->identity().id()) != entity.slot ||
		soldier->identity().incarnation() != entity.incarnation)
		return nullptr;
	return soldier;
}

TacticalEntityId GetJa2TacticalEntityId(std::uint16_t slot) noexcept
{
	const TacticalEntityId entity = BoundDirectory()->identity(slot);
	return ResolveJa2TacticalEntity(entity) ? entity : TacticalEntityId{};
}

TacticalEntityId GetJa2TacticalEntityId(
	const TacticalActor& soldier) noexcept
{
	const TacticalEntityId entity = GetJa2TacticalEntityId(
		static_cast<std::uint16_t>(soldier.identity().id()));
	return ResolveJa2TacticalEntity(entity) == &soldier
		? entity
		: TacticalEntityId{};
}

void ResetJa2TacticalActorRosters() noexcept
{
	ActiveActorRoster().clear();
	AwayActorRoster().clear();
}

std::size_t Ja2ActiveTacticalActorSlotCount() noexcept
{
	return ActiveActorRoster().highWaterMark();
}

std::size_t Ja2AwayTacticalActorSlotCount() noexcept
{
	return AwayActorRoster().highWaterMark();
}

TacticalActor* ResolveJa2ActiveTacticalActorSlot(
	std::size_t rosterSlot) noexcept
{
	return ResolveJa2TacticalEntity(
		ActiveActorRoster().actor(rosterSlot));
}

TacticalActor* ResolveJa2AwayTacticalActorSlot(
	std::size_t rosterSlot) noexcept
{
	return ResolveJa2TacticalEntity(
		AwayActorRoster().actor(rosterSlot));
}

std::int32_t AddJa2ActiveTacticalActor(
	TacticalEntityId actor) noexcept
{
	const std::int32_t slot =
		AddActor(ActiveActorRoster(), actor);
	if (slot >= 0) (void)AwayActorRoster().erase(actor);
	return slot;
}

std::int32_t AddJa2AwayTacticalActor(
	TacticalEntityId actor) noexcept
{
	const std::int32_t slot =
		AddActor(AwayActorRoster(), actor);
	if (slot >= 0) (void)ActiveActorRoster().erase(actor);
	return slot;
}

bool RemoveJa2ActiveTacticalActor(
	TacticalEntityId actor) noexcept
{
	return ActiveActorRoster().erase(actor);
}

bool RemoveJa2AwayTacticalActor(
	TacticalEntityId actor) noexcept
{
	return AwayActorRoster().erase(actor);
}

bool Ja2TacticalEntityReference::capture(
	TacticalEntityId entity) noexcept
{
	reset();
	if (!entity.valid() || !ResolveJa2TacticalEntity(entity))
		return false;
	entity_ = entity;
	return true;
}

TacticalActor* Ja2TacticalEntityReference::resolve() const noexcept
{
	return ResolveJa2TacticalEntity(entity_);
}

TacticalActor* Ja2TacticalEntityReference::consume() noexcept
{
	TacticalActor* soldier = resolve();
	reset();
	return soldier;
}
