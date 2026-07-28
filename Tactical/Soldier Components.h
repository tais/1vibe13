#ifndef TACTICAL_SOLDIER_COMPONENTS_H
#define TACTICAL_SOLDIER_COMPONENTS_H

#include "Disease Types.h"
#include "Overhead Types.h"
#include "types.h"

#include <functional>

// Stable indices for persistent critical-stat damage. Keep the order aligned
// with the established soldier save fields.
enum
{
	DAMAGED_STAT_HEALTH,
	DAMAGED_STAT_DEXTERITY,
	DAMAGED_STAT_AGILITY,
	DAMAGED_STAT_STRENGTH,
	DAMAGED_STAT_WISDOM,
	DAMAGED_STAT_LEADERSHIP,
	DAMAGED_STAT_MARKSMANSHIP,
	DAMAGED_STAT_MECHANICAL,
	DAMAGED_STAT_EXPLOSIVES,
	DAMAGED_STAT_MEDICAL,
	NUM_DAMAGABLE_STATS,
};

// Stable indices for persistent skill/trait counters and heterogeneous
// cooldown values. The unused capacity is part of the established save schema.
enum
{
	// Prevent one AI operator from ordering several artillery strikes at once.
	SOLDIER_COUNTER_RADIO_ARTILLERY,
	// Track how long the soldier has prepared as a spotter.
	SOLDIER_COUNTER_SPOTTER,
	// Accumulate turns in which the player observes an enemy's role.
	SOLDIER_COUNTER_ROLE_OBSERVED,
	// Track retreating from the current position.
	SOLDIER_COUNTER_RETREAT,

	SOLDIER_COUNTER_MAX = 20,
};

enum
{
	SOLDIER_COOLDOWN_COVERTOPS_TEMPORARYOVERT_SECONDS = 0,
	SOLDIER_COOLDOWN_COVERTOPS_TEMPORARYOVERT_APS,
	// Count turns for which a character remains frozen.
	SOLDIER_COOLDOWN_CRYO,
	// Block intel gathering for a number of strategic hours after discovery.
	SOLDIER_COOLDOWN_INTEL_PENALTY,
	// Delay autonomous drug use after deliberate combat use.
	SOLDIER_COOLDOWN_DRUGUSER_COMBAT,
	// Rate-limit the robot's X-ray detector.
	SOLDIER_COOLDOWN_ROBOT_XRAY,

	SOLDIER_COOLDOWN_MAX = 20,
};

// Fixed assist-attribution capacity from the established soldier save schema.
// Soldier IDs outside this historical player-team range have no persisted slot.
enum
{
	NUM_ASSIST_SLOTS = 156,
};

// Fixed patrol capacity from the established soldier save schema.
enum
{
	SOLDIER_PATROL_GRID_COUNT = 10,
};

// Fixed tactical display-name capacity from the established soldier schema.
enum
{
	SOLDIER_NAME_LENGTH = 10,
};

// Stable indices and capacity for persistent drug effects. The unused slots
// are part of the established save schema and remain serialized.
enum
{
	DRUG_EFFECT_HP = 0,
	DRUG_EFFECT_BP,
	DRUG_EFFECT_AP,
	DRUG_EFFECT_MORALE,
	DRUG_EFFECT_PHYS_RES,
	DRUG_EFFECT_STR,
	DRUG_EFFECT_AGI,
	DRUG_EFFECT_DEX,
	DRUG_EFFECT_WIS,

	DRUG_EFFECT_MAX = 20,
};

// Canonical identity of one soldier incarnation. Slot identity, display name,
// physical archetype, profile links, and persistent militia identity belong
// together but remain independent from roster membership and mutable status.
class SoldierIdentityComponent
{
public:
	using Name = CHAR16[SOLDIER_NAME_LENGTH];

	SoldierID& id() noexcept { return id_; }
	const SoldierID& id() const noexcept { return id_; }
	Name& name() noexcept { return name_; }
	const Name& name() const noexcept { return name_; }
	UINT8& bodyType() noexcept { return bodyType_; }
	const UINT8& bodyType() const noexcept { return bodyType_; }
	UINT8& profile() noexcept { return profile_; }
	const UINT8& profile() const noexcept { return profile_; }
	UINT32& incarnation() noexcept { return incarnation_; }
	const UINT32& incarnation() const noexcept { return incarnation_; }
	UINT16& dataProfile() noexcept { return dataProfile_; }
	const UINT16& dataProfile() const noexcept { return dataProfile_; }
	UINT32& individualMilitiaId() noexcept { return individualMilitiaId_; }
	const UINT32& individualMilitiaId() const noexcept { return individualMilitiaId_; }

	bool hasIncarnation() const noexcept { return incarnation_ != 0; }
	void reset() noexcept;

private:
	SoldierID id_{ static_cast<UINT16>(0) };
	Name name_{};
	UINT8 bodyType_ = 0;
	UINT8 profile_ = 0;
	UINT32 incarnation_ = 0;
	UINT16 dataProfile_ = 0;
	UINT32 individualMilitiaId_ = 0;
};

// Canonical tactical-roster membership and allegiance. Allocation/activity,
// team and side, sector presence, tactical class, and civilian group move
// together without becoming permanent identity or strategic deployment data.
class SoldierRosterComponent
{
public:
	INT8& active() noexcept { return active_; }
	const INT8& active() const noexcept { return active_; }
	INT8& team() noexcept { return team_; }
	const INT8& team() const noexcept { return team_; }
	UINT8& inSector() noexcept { return inSector_; }
	const UINT8& inSector() const noexcept { return inSector_; }
	UINT8& side() noexcept { return side_; }
	const UINT8& side() const noexcept { return side_; }
	UINT8& soldierClass() noexcept { return soldierClass_; }
	const UINT8& soldierClass() const noexcept { return soldierClass_; }
	UINT8& civilianGroup() noexcept { return civilianGroup_; }
	const UINT8& civilianGroup() const noexcept { return civilianGroup_; }

	bool isActive() const noexcept { return active_ != 0; }
	bool isInSector() const noexcept { return inSector_ != 0; }
	void reset() noexcept;

private:
	INT8 active_ = 0;
	INT8 team_ = 0;
	UINT8 inSector_ = 0;
	UINT8 side_ = 0;
	UINT8 soldierClass_ = 0;
	UINT8 civilianGroup_ = 0;
};

// Canonical soldier vitals and recovery storage. Reference accessors keep
// legacy mutation sites zero-cost while health, breath, treatable trauma,
// surgery, critical-stat damage, and bleed timing share one reset boundary.
class SoldierVitalsComponent
{
public:
	using CriticalStatDamage = UINT8[NUM_DAMAGABLE_STATS];

	INT8& health() noexcept { return health_; }
	const INT8& health() const noexcept { return health_; }
	INT8& maximumHealth() noexcept { return maximumHealth_; }
	const INT8& maximumHealth() const noexcept { return maximumHealth_; }
	INT8& breath() noexcept { return breath_; }
	const INT8& breath() const noexcept { return breath_; }
	INT8& maximumBreath() noexcept { return maximumBreath_; }
	const INT8& maximumBreath() const noexcept { return maximumBreath_; }
	INT8& bleeding() noexcept { return bleeding_; }
	const INT8& bleeding() const noexcept { return bleeding_; }
	INT8& previousHealth() noexcept { return previousHealth_; }
	const INT8& previousHealth() const noexcept { return previousHealth_; }
	INT16& fractionalHealth() noexcept { return fractionalHealth_; }
	const INT16& fractionalHealth() const noexcept { return fractionalHealth_; }
	INT16& breathReduction() noexcept { return breathReduction_; }
	const INT16& breathReduction() const noexcept { return breathReduction_; }
	INT32& healableInjury() noexcept { return healableInjury_; }
	const INT32& healableInjury() const noexcept { return healableInjury_; }
	BOOLEAN& undergoingSurgery() noexcept { return undergoingSurgery_; }
	const BOOLEAN& undergoingSurgery() const noexcept { return undergoingSurgery_; }
	signed long& unregainableBreath() noexcept { return unregainableBreath_; }
	const signed long& unregainableBreath() const noexcept { return unregainableBreath_; }
	CriticalStatDamage& criticalStatDamage() noexcept { return criticalStatDamage_; }
	const CriticalStatDamage& criticalStatDamage() const noexcept { return criticalStatDamage_; }
	FLOAT& nextBleedAt() noexcept { return nextBleedAt_; }
	const FLOAT& nextBleedAt() const noexcept { return nextBleedAt_; }
	INT8& regenerationCounter() noexcept { return regenerationCounter_; }
	const INT8& regenerationCounter() const noexcept { return regenerationCounter_; }
	INT8& regenerationBoostersUsedToday() noexcept { return regenerationBoostersUsedToday_; }
	const INT8& regenerationBoostersUsedToday() const noexcept { return regenerationBoostersUsedToday_; }
	INT32& lastBleedGruntAt() noexcept { return lastBleedGruntAt_; }
	const INT32& lastBleedGruntAt() const noexcept { return lastBleedGruntAt_; }

	bool alive() const noexcept;
	bool hasHealableInjury() const noexcept { return healableInjury_ > 0; }
	bool isUndergoingSurgery() const noexcept { return undergoingSurgery_ != FALSE; }
	void snapshotHealth() noexcept { previousHealth_ = health_; }
	void beginSurgery() noexcept { undergoingSurgery_ = TRUE; }
	void finishSurgery() noexcept { undergoingSurgery_ = FALSE; }
	void clearCriticalStatDamage() noexcept;
	void applyLifeDeduction(INT16 lifeDeduction);
	void reset() noexcept;

private:
	INT8 health_ = 0;
	INT8 maximumHealth_ = 0;
	INT8 breath_ = 0;
	INT8 maximumBreath_ = 0;
	INT8 bleeding_ = 0;
	INT8 previousHealth_ = 0;
	INT16 fractionalHealth_ = 0;
	INT16 breathReduction_ = 0;
	INT32 healableInjury_ = 0;
	BOOLEAN undergoingSurgery_ = FALSE;
	signed long unregainableBreath_ = 0;
	CriticalStatDamage criticalStatDamage_ = {};
	FLOAT nextBleedAt_ = 0;
	INT8 regenerationCounter_ = 0;
	INT8 regenerationBoostersUsedToday_ = 0;
	INT32 lastBleedGruntAt_ = 0;
};

// Canonical base attributes and learned trait slots. Reference accessors keep
// the established hot-path mutations zero-cost while one owner defines the
// complete reset and persistent-capacity boundary.
class SoldierStatisticsComponent
{
public:
	static constexpr UINT8 SkillTraitCapacity = 30;
	using SkillTraits = UINT8[SkillTraitCapacity];

	INT8& experienceLevel() noexcept { return experienceLevel_; }
	const INT8& experienceLevel() const noexcept { return experienceLevel_; }
	INT8& strength() noexcept { return strength_; }
	const INT8& strength() const noexcept { return strength_; }
	INT8& agility() noexcept { return agility_; }
	const INT8& agility() const noexcept { return agility_; }
	INT8& dexterity() noexcept { return dexterity_; }
	const INT8& dexterity() const noexcept { return dexterity_; }
	INT8& wisdom() noexcept { return wisdom_; }
	const INT8& wisdom() const noexcept { return wisdom_; }
	INT8& leadership() noexcept { return leadership_; }
	const INT8& leadership() const noexcept { return leadership_; }
	INT8& marksmanship() noexcept { return marksmanship_; }
	const INT8& marksmanship() const noexcept { return marksmanship_; }
	INT8& mechanical() noexcept { return mechanical_; }
	const INT8& mechanical() const noexcept { return mechanical_; }
	INT8& explosives() noexcept { return explosives_; }
	const INT8& explosives() const noexcept { return explosives_; }
	INT8& medical() noexcept { return medical_; }
	const INT8& medical() const noexcept { return medical_; }
	INT8& scientific() noexcept { return scientific_; }
	const INT8& scientific() const noexcept { return scientific_; }
	UINT8& skillTrait(UINT8 index) noexcept { return skillTraits_[index]; }
	const UINT8& skillTrait(UINT8 index) const noexcept
	{
		return skillTraits_[index];
	}

	void reset() noexcept;

private:
	INT8 experienceLevel_ = 0;
	INT8 strength_ = 0;
	INT8 agility_ = 0;
	INT8 dexterity_ = 0;
	INT8 wisdom_ = 0;
	INT8 leadership_ = 0;
	INT8 marksmanship_ = 0;
	INT8 mechanical_ = 0;
	INT8 explosives_ = 0;
	INT8 medical_ = 0;
	INT8 scientific_ = 0;
	SkillTraits skillTraits_{};
};

// Canonical owner for the established general-purpose soldier bit mask. The
// mask remains value-compatible while callers transition from public aggregate
// storage to explicit status queries and mutations.
class SoldierStatusComponent
{
public:
	UINT32& flags() noexcept { return flags_; }
	const UINT32& flags() const noexcept { return flags_; }

	bool hasAny(UINT32 flags) const noexcept { return (flags_ & flags) != 0; }
	void set(UINT32 flags) noexcept { flags_ |= flags; }
	void clear(UINT32 flags) noexcept { flags_ &= ~flags; }
	void reset() noexcept;

private:
	UINT32 flags_ = 0;
};

// Canonical persistent inventory-adjacent state. Key access, new-item refresh,
// and load-bearing-equipment zipper/drop-pack flags share the inventory
// lifecycle without exposing another generic soldier flag bucket.
class SoldierInventoryStateComponent
{
public:
	INT8& keyAccess() noexcept { return keyAccess_; }
	const INT8& keyAccess() const noexcept { return keyAccess_; }
	BOOLEAN& checkForNewItems() noexcept { return checkForNewItems_; }
	const BOOLEAN& checkForNewItems() const noexcept { return checkForNewItems_; }
	BOOLEAN& zipperFlag() noexcept { return zipperFlag_; }
	const BOOLEAN& zipperFlag() const noexcept { return zipperFlag_; }
	BOOLEAN& dropPackFlag() noexcept { return dropPackFlag_; }
	const BOOLEAN& dropPackFlag() const noexcept { return dropPackFlag_; }

	void reset() noexcept;

private:
	INT8 keyAccess_ = 0;
	BOOLEAN checkForNewItems_ = FALSE;
	BOOLEAN zipperFlag_ = FALSE;
	BOOLEAN dropPackFlag_ = FALSE;
};

// Canonical tactical service relationship. A provider points at one patient,
// while the patient counts all active providers and may reserve one medic
// during automatic bandaging. The persisted activity marker is retained
// separately because old saves and AI target selection still observe it.
class SoldierServiceComponent
{
public:
	INT8& activity() noexcept { return activity_; }
	const INT8& activity() const noexcept { return activity_; }
	UINT8& providerCount() noexcept { return providerCount_; }
	const UINT8& providerCount() const noexcept { return providerCount_; }
	SoldierID& partner() noexcept { return partner_; }
	const SoldierID& partner() const noexcept { return partner_; }
	SoldierID& autoBandagingMedic() noexcept { return autoBandagingMedic_; }
	const SoldierID& autoBandagingMedic() const noexcept { return autoBandagingMedic_; }
	INT8& borrowedInventorySlot() noexcept { return borrowedInventorySlot_; }
	const INT8& borrowedInventorySlot() const noexcept { return borrowedInventorySlot_; }

	bool active() const noexcept { return activity_ != 0; }
	bool hasProviders() const noexcept { return providerCount_ != 0; }
	bool hasPartner() const noexcept { return partner_ != NOBODY; }
	bool hasAutoBandagingMedic() const noexcept { return autoBandagingMedic_ != NOBODY; }
	void beginProvidingTo(SoldierID patient) noexcept { partner_ = patient; }
	void finishProviding() noexcept { partner_ = NOBODY; }
	void addProvider() noexcept;
	void removeProvider() noexcept;
	void clearProviders() noexcept { providerCount_ = 0; }
	void assignAutoBandagingMedic(SoldierID medic) noexcept { autoBandagingMedic_ = medic; }
	void clearAutoBandagingMedic() noexcept { autoBandagingMedic_ = NOBODY; }
	void borrowInventorySlot(INT8 slot) noexcept { borrowedInventorySlot_ = slot; }
	void clearBorrowedInventorySlot() noexcept { borrowedInventorySlot_ = -1; }
	void reset() noexcept;

private:
	INT8 activity_ = 0;
	UINT8 providerCount_ = 0;
	SoldierID partner_ = NOBODY;
	SoldierID autoBandagingMedic_ = NOBODY;
	INT8 borrowedInventorySlot_ = 0;
};

// Canonical spoken-dialogue state. NPC quote plans, quote-history masks,
// battle-voice selection and playback throttling, queued tactical feedback,
// civilian quote progression, speech cooldowns, and corpse-comment tolerance
// share one reset boundary.
// World-position and mechanical-loop sounds deliberately remain outside this
// component because they are spatial audio rather than soldier speech.
class SoldierDialogueComponent
{
public:
	UINT8& quoteRecord() noexcept { return quoteRecord_; }
	const UINT8& quoteRecord() const noexcept { return quoteRecord_; }
	UINT8& quoteActionId() noexcept { return quoteActionId_; }
	const UINT8& quoteActionId() const noexcept { return quoteActionId_; }
	UINT8& battleSoundSet() noexcept { return battleSoundSet_; }
	const UINT8& battleSoundSet() const noexcept { return battleSoundSet_; }
	UINT16& saidFlags() noexcept { return saidFlags_; }
	const UINT16& saidFlags() const noexcept { return saidFlags_; }
	INT8& vocalVolume() noexcept { return vocalVolume_; }
	const INT8& vocalVolume() const noexcept { return vocalVolume_; }
	UINT32& repeatedBattleSoundAt() noexcept { return repeatedBattleSoundAt_; }
	const UINT32& repeatedBattleSoundAt() const noexcept { return repeatedBattleSoundAt_; }
	INT8& previousBattleSound() noexcept { return previousBattleSound_; }
	const INT8& previousBattleSound() const noexcept { return previousBattleSound_; }
	UINT8& heardNoiseCooldownTurns() noexcept { return heardNoiseCooldownTurns_; }
	const UINT8& heardNoiseCooldownTurns() const noexcept { return heardNoiseCooldownTurns_; }
	UINT16& saidExtendedFlags() noexcept { return saidExtendedFlags_; }
	const UINT16& saidExtendedFlags() const noexcept { return saidExtendedFlags_; }
	UINT32& activeBattleSound() noexcept { return activeBattleSound_; }
	const UINT32& activeBattleSound() const noexcept { return activeBattleSound_; }
	INT8& currentCivilianQuote() noexcept { return currentCivilianQuote_; }
	const INT8& currentCivilianQuote() const noexcept { return currentCivilianQuote_; }
	INT8& civilianQuoteDelta() noexcept { return civilianQuoteDelta_; }
	const INT8& civilianQuoteDelta() const noexcept { return civilianQuoteDelta_; }
	UINT32& lastSpokeAt() noexcept { return lastSpokeAt_; }
	const UINT32& lastSpokeAt() const noexcept { return lastSpokeAt_; }
	INT8& corpseQuoteTolerance() noexcept { return corpseQuoteTolerance_; }
	const INT8& corpseQuoteTolerance() const noexcept { return corpseQuoteTolerance_; }
	BOOLEAN& deadSoundPlayedState() noexcept { return deadSoundPlayed_; }
	const BOOLEAN& deadSoundPlayedState() const noexcept { return deadSoundPlayed_; }
	BOOLEAN& bleedingWarningSpokenState() noexcept { return bleedingWarningSpoken_; }
	const BOOLEAN& bleedingWarningSpokenState() const noexcept { return bleedingWarningSpoken_; }
	BOOLEAN& dyingCommentSpokenState() noexcept { return dyingCommentSpoken_; }
	const BOOLEAN& dyingCommentSpokenState() const noexcept { return dyingCommentSpoken_; }
	BOOLEAN& ammoQuotePendingState() noexcept { return ammoQuotePending_; }
	const BOOLEAN& ammoQuotePendingState() const noexcept { return ammoQuotePending_; }
	BOOLEAN& dieSoundUsedState() noexcept { return dieSoundUsed_; }
	const BOOLEAN& dieSoundUsedState() const noexcept { return dieSoundUsed_; }

	bool hasQuoteRecord() const noexcept { return quoteRecord_ != 0; }
	bool hasQuoteAction() const noexcept { return quoteActionId_ != 0; }
	bool deathSoundPlayed() const noexcept { return deadSoundPlayed_ != FALSE; }
	bool hasWarnedAboutBleeding() const noexcept { return bleedingWarningSpoken_ != FALSE; }
	bool hasMadeDyingComment() const noexcept { return dyingCommentSpoken_ != FALSE; }
	bool ammoQuotePending() const noexcept { return ammoQuotePending_ != FALSE; }
	bool deathBattleSoundUsed() const noexcept { return dieSoundUsed_ != FALSE; }
	bool hasSaid(UINT16 flag) const noexcept { return (saidFlags_ & flag) != 0; }
	bool hasSaidExtended(UINT16 flag) const noexcept
	{
		return (saidExtendedFlags_ & flag) != 0;
	}
	void markSaid(UINT16 flag) noexcept { saidFlags_ |= flag; }
	void clearSaid(UINT16 flag) noexcept { saidFlags_ &= static_cast<UINT16>(~flag); }
	void markSaidExtended(UINT16 flag) noexcept { saidExtendedFlags_ |= flag; }
	void clearSaidExtended(UINT16 flag) noexcept
	{
		saidExtendedFlags_ &= static_cast<UINT16>(~flag);
	}
	void clearQuotePlan() noexcept
	{
		quoteRecord_ = 0;
		quoteActionId_ = 0;
	}
	void recordBattleSound(INT8 sound, UINT32 now) noexcept
	{
		previousBattleSound_ = sound;
		repeatedBattleSoundAt_ = now;
	}
	void startHeardNoiseCooldown(UINT8 turns) noexcept
	{
		heardNoiseCooldownTurns_ = turns;
	}
	void ageHeardNoiseCooldown() noexcept
	{
		if (heardNoiseCooldownTurns_ > 0)
		{
			--heardNoiseCooldownTurns_;
		}
	}
	void clearCivilianQuote() noexcept
	{
		currentCivilianQuote_ = -1;
		civilianQuoteDelta_ = 0;
	}
	void recordSpokeAt(UINT32 now) noexcept { lastSpokeAt_ = now; }
	void markDeathSoundPlayed() noexcept { deadSoundPlayed_ = TRUE; }
	void clearDeathSoundPlayed() noexcept { deadSoundPlayed_ = FALSE; }
	void markBleedingWarningSpoken() noexcept { bleedingWarningSpoken_ = TRUE; }
	void clearBleedingWarning() noexcept { bleedingWarningSpoken_ = FALSE; }
	void markDyingCommentSpoken() noexcept { dyingCommentSpoken_ = TRUE; }
	void clearDyingComment() noexcept { dyingCommentSpoken_ = FALSE; }
	void queueAmmoQuote() noexcept { ammoQuotePending_ = TRUE; }
	void consumeAmmoQuote() noexcept { ammoQuotePending_ = FALSE; }
	void markDeathBattleSoundUsed() noexcept { dieSoundUsed_ = TRUE; }
	void clearDeathBattleSoundUsed() noexcept { dieSoundUsed_ = FALSE; }
	void reset() noexcept;

private:
	UINT8 quoteRecord_ = 0;
	UINT8 quoteActionId_ = 0;
	UINT8 battleSoundSet_ = 0;
	UINT16 saidFlags_ = 0;
	INT8 vocalVolume_ = 0;
	UINT32 repeatedBattleSoundAt_ = 0;
	INT8 previousBattleSound_ = 0;
	UINT8 heardNoiseCooldownTurns_ = 0;
	UINT16 saidExtendedFlags_ = 0;
	UINT32 activeBattleSound_ = 0;
	INT8 currentCivilianQuote_ = 0;
	INT8 civilianQuoteDelta_ = 0;
	UINT32 lastSpokeAt_ = 0;
	INT8 corpseQuoteTolerance_ = 0;
	BOOLEAN deadSoundPlayed_ = FALSE;
	BOOLEAN bleedingWarningSpoken_ = FALSE;
	BOOLEAN dyingCommentSpoken_ = FALSE;
	BOOLEAN ammoQuotePending_ = FALSE;
	BOOLEAN dieSoundUsed_ = FALSE;
};

// Canonical non-dialogue audio state. Footstep variation and door noise are
// gameplay-facing sound values; burst, positional, and turret-turning IDs are
// opaque handles owned by the legacy sound adapter. Keeping their lifetime
// together prevents stopped handles from remaining live soldier state.
class SoldierAudioComponent
{
public:
	static constexpr INT32 NoSample = -1;

	UINT8& lastFootstepVariant() noexcept { return lastFootstepVariant_; }
	const UINT8& lastFootstepVariant() const noexcept { return lastFootstepVariant_; }
	UINT8& doorOpeningNoise() noexcept { return doorOpeningNoise_; }
	const UINT8& doorOpeningNoise() const noexcept { return doorOpeningNoise_; }
	INT32& burstSoundId() noexcept { return burstSoundId_; }
	const INT32& burstSoundId() const noexcept { return burstSoundId_; }
	INT32& positionSoundId() noexcept { return positionSoundId_; }
	const INT32& positionSoundId() const noexcept { return positionSoundId_; }
	INT32& turningSoundId() noexcept { return turningSoundId_; }
	const INT32& turningSoundId() const noexcept { return turningSoundId_; }

	bool hasDoorOpeningNoise() const noexcept { return doorOpeningNoise_ != 0; }
	bool hasBurstSound() const noexcept { return burstSoundId_ != NoSample; }
	bool hasPositionSound() const noexcept { return positionSoundId_ != NoSample; }
	bool hasTurningSound() const noexcept { return turningSoundId_ != NoSample; }
	void recordFootstepVariant(UINT8 variant) noexcept { lastFootstepVariant_ = variant; }
	void recordDoorOpeningNoise(UINT8 volume) noexcept { doorOpeningNoise_ = volume; }
	void clearDoorOpeningNoise() noexcept { doorOpeningNoise_ = 0; }
	void startBurstSound(INT32 soundId) noexcept { burstSoundId_ = soundId; }
	void clearBurstSound() noexcept { burstSoundId_ = NoSample; }
	void startPositionSound(INT32 soundId) noexcept { positionSoundId_ = soundId; }
	void clearPositionSound() noexcept { positionSoundId_ = NoSample; }
	void startTurningSound(INT32 soundId) noexcept { turningSoundId_ = soundId; }
	void clearTurningSound() noexcept { turningSoundId_ = NoSample; }
	void reset() noexcept;

private:
	UINT8 lastFootstepVariant_ = 0;
	UINT8 doorOpeningNoise_ = 0;
	INT32 burstSoundId_ = NoSample;
	INT32 positionSoundId_ = NoSample;
	INT32 turningSoundId_ = NoSample;
};

// Canonical network-replication bookkeeping. Movement/update timestamps,
// sequence metadata, scheduled synchronization stops, and the persisted
// integrity checksum share one lifecycle without leaking transport details
// into the rest of the soldier model.
class SoldierReplicationComponent
{
public:
	UINT32& movementStartedAt() noexcept { return movementStartedAt_; }
	const UINT32& movementStartedAt() const noexcept { return movementStartedAt_; }
	UINT32& optimumMovementTime() noexcept { return optimumMovementTime_; }
	const UINT32& optimumMovementTime() const noexcept { return optimumMovementTime_; }
	UINT32& lastUpdateAt() noexcept { return lastUpdateAt_; }
	const UINT32& lastUpdateAt() const noexcept { return lastUpdateAt_; }
	UINT32& updateSequence() noexcept { return updateSequence_; }
	const UINT32& updateSequence() const noexcept { return updateSequence_; }
	UINT8& updateType() noexcept { return updateType_; }
	const UINT8& updateType() const noexcept { return updateType_; }
	INT32& scheduledStopGrid() noexcept { return scheduledStopGrid_; }
	const INT32& scheduledStopGrid() const noexcept { return scheduledStopGrid_; }
	UINT32& checksum() noexcept { return checksum_; }
	const UINT32& checksum() const noexcept { return checksum_; }
	BOOLEAN& updatedFromNetwork() noexcept { return updatedFromNetwork_; }
	const BOOLEAN& updatedFromNetwork() const noexcept { return updatedFromNetwork_; }

	bool hasLastUpdate() const noexcept { return lastUpdateAt_ != 0; }
	bool updateTimedOut(UINT32 now, UINT32 timeout) const noexcept
	{
		return hasLastUpdate() && (now - lastUpdateAt_) > timeout;
	}
	void recordUpdate(UINT32 now) noexcept { lastUpdateAt_ = now; }
	void scheduleStop(INT32 grid) noexcept { scheduledStopGrid_ = grid; }
	void clearScheduledStop() noexcept { scheduledStopGrid_ = 0; }
	void recordChecksum(UINT32 checksum) noexcept { checksum_ = checksum; }
	void reset() noexcept;

private:
	UINT32 movementStartedAt_ = 0;
	UINT32 optimumMovementTime_ = 0;
	UINT32 lastUpdateAt_ = 0;
	UINT32 updateSequence_ = 0;
	UINT8 updateType_ = 0;
	INT32 scheduledStopGrid_ = 0;
	UINT32 checksum_ = 0;
	BOOLEAN updatedFromNetwork_ = FALSE;
};

// Canonical movement telemetry consumed by turn rules, visibility, accuracy,
// suppression, medical estimates, and realtime breath updates. Recording tile
// movement saturates the narrow persisted counters instead of allowing a long
// route to wrap them negative or back to zero.
class SoldierMovementMetricsComponent
{
public:
	static constexpr INT8 MaximumTurnTiles = 127;
	static constexpr UINT8 MaximumRealtimeBreathTiles = 255;

	INT16& carriedWeightAtTurnStart() noexcept { return carriedWeightAtTurnStart_; }
	const INT16& carriedWeightAtTurnStart() const noexcept { return carriedWeightAtTurnStart_; }
	INT8& tilesMoved() noexcept { return tilesMoved_; }
	const INT8& tilesMoved() const noexcept { return tilesMoved_; }
	UINT8& realtimeBreathTiles() noexcept { return realtimeBreathTiles_; }
	const UINT8& realtimeBreathTiles() const noexcept { return realtimeBreathTiles_; }
	UINT16& lastRealtimeMovementAnimation() noexcept { return lastRealtimeMovementAnimation_; }
	const UINT16& lastRealtimeMovementAnimation() const noexcept { return lastRealtimeMovementAnimation_; }

	bool movedThisTurn() const noexcept { return tilesMoved_ != 0; }
	bool hasRealtimeBreathMovement() const noexcept { return realtimeBreathTiles_ != 0; }
	void recordCarriedWeightAtTurnStart(INT16 weight) noexcept { carriedWeightAtTurnStart_ = weight; }
	void recordTileMovement(bool running, bool realtime, UINT16 animation) noexcept;
	void clearTurnDistance() noexcept { tilesMoved_ = 0; }
	void clearRealtimeBreathMovement() noexcept { realtimeBreathTiles_ = 0; }
	void reset() noexcept;

private:
	INT16 carriedWeightAtTurnStart_ = 0;
	INT8 tilesMoved_ = 0;
	UINT8 realtimeBreathTiles_ = 0;
	UINT16 lastRealtimeMovementAnimation_ = 0;
};

// Canonical tactical-AI action plan for one actor. Current/queued actions,
// patrol progress, aiming cadence, flanking progress, sniper posture, and
// modular plan selection share one reset boundary. The component owns state,
// not AI policy or installed content.
class SoldierAiPlanningComponent
{
public:
	using PatrolGrid = INT32[SOLDIER_PATROL_GRID_COUNT];
	static constexpr INT8 MaximumFlankCount = 127;

	INT8& lastAction() noexcept { return lastAction_; }
	const INT8& lastAction() const noexcept { return lastAction_; }
	INT8& action() noexcept { return action_; }
	const INT8& action() const noexcept { return action_; }
	INT32& actionData() noexcept { return actionData_; }
	const INT32& actionData() const noexcept { return actionData_; }
	INT8& nextAction() noexcept { return nextAction_; }
	const INT8& nextAction() const noexcept { return nextAction_; }
	INT32& nextActionData() noexcept { return nextActionData_; }
	const INT32& nextActionData() const noexcept { return nextActionData_; }
	INT8& actionInProgress() noexcept { return actionInProgress_; }
	const INT8& actionInProgress() const noexcept { return actionInProgress_; }
	INT8& nextTargetLevel() noexcept { return nextTargetLevel_; }
	const INT8& nextTargetLevel() const noexcept { return nextTargetLevel_; }
	INT8& dominantDirection() noexcept { return dominantDirection_; }
	const INT8& dominantDirection() const noexcept { return dominantDirection_; }
	INT8& patrolCount() noexcept { return patrolCount_; }
	const INT8& patrolCount() const noexcept { return patrolCount_; }
	INT8& nextPatrolPoint() noexcept { return nextPatrolPoint_; }
	const INT8& nextPatrolPoint() const noexcept { return nextPatrolPoint_; }
	PatrolGrid& patrolGrid() noexcept { return patrolGrid_; }
	const PatrolGrid& patrolGrid() const noexcept { return patrolGrid_; }
	INT16& aimTime() noexcept { return aimTime_; }
	const INT16& aimTime() const noexcept { return aimTime_; }
	INT8& shownAimTime() noexcept { return shownAimTime_; }
	const INT8& shownAimTime() const noexcept { return shownAimTime_; }
	INT8& flankCount() noexcept { return flankCount_; }
	const INT8& flankCount() const noexcept { return flankCount_; }
	INT32& flankAnchorGrid() noexcept { return flankAnchorGrid_; }
	const INT32& flankAnchorGrid() const noexcept { return flankAnchorGrid_; }
	INT8& sniperPosture() noexcept { return sniperPosture_; }
	const INT8& sniperPosture() const noexcept { return sniperPosture_; }
	INT16& flankOriginDirection() noexcept { return flankOriginDirection_; }
	const INT16& flankOriginDirection() const noexcept { return flankOriginDirection_; }
	INT16& planIndex() noexcept { return planIndex_; }
	const INT16& planIndex() const noexcept { return planIndex_; }
	BOOLEAN& lastFlankLeft() noexcept { return lastFlankLeft_; }
	const BOOLEAN& lastFlankLeft() const noexcept { return lastFlankLeft_; }

	bool flanking(INT8 terminalCount) const noexcept
	{
		return flankCount_ > 0 && flankCount_ < terminalCount;
	}
	bool hasActionInProgress() const noexcept { return actionInProgress_ != 0; }
	bool hasPatrolRoute() const noexcept { return patrolCount_ > 0; }
	bool sniperPostureActive() const noexcept { return sniperPosture_ != 0; }
	bool hasPlanIndex() const noexcept { return planIndex_ != 0; }
	void queueAction(INT8 action, INT32 data) noexcept
	{
		nextAction_ = action;
		nextActionData_ = data;
	}
	void recordFlankStep(INT32 anchorGrid, INT16 originDirection) noexcept;
	void advanceFlank() noexcept;
	void finishFlank(INT8 terminalCount) noexcept { flankCount_ = terminalCount; }
	void clearFlank() noexcept { flankCount_ = 0; }
	void raiseSniperPosture() noexcept { sniperPosture_ = 1; }
	void lowerSniperPosture() noexcept { sniperPosture_ = 0; }
	INT16 ensurePlanIndex(INT16 fallback) noexcept;
	void reset() noexcept;

private:
	INT8 lastAction_ = 0;
	INT8 action_ = 0;
	INT32 actionData_ = 0;
	INT8 nextAction_ = 0;
	INT32 nextActionData_ = 0;
	INT8 actionInProgress_ = 0;
	INT8 nextTargetLevel_ = 0;
	INT8 dominantDirection_ = 0;
	INT8 patrolCount_ = 0;
	INT8 nextPatrolPoint_ = 0;
	PatrolGrid patrolGrid_{};
	INT16 aimTime_ = 0;
	INT8 shownAimTime_ = 0;
	INT8 flankCount_ = 0;
	INT32 flankAnchorGrid_ = 0;
	INT8 sniperPosture_ = 0;
	INT16 flankOriginDirection_ = 0;
	INT16 planIndex_ = 0;
	BOOLEAN lastFlankLeft_ = FALSE;
};

// Canonical tactical-AI behavioral mode. Alertness, orders, attitude, escort
// status, scheduling flags, and creature movement modes are independent from
// the selected action and from sensory knowledge.
class SoldierAiBehaviorComponent
{
public:
	INT8& alertStatus() noexcept { return alertStatus_; }
	const INT8& alertStatus() const noexcept { return alertStatus_; }
	INT8& neutral() noexcept { return neutral_; }
	const INT8& neutral() const noexcept { return neutral_; }
	INT8& newSituation() noexcept { return newSituation_; }
	const INT8& newSituation() const noexcept { return newSituation_; }
	INT8& orders() noexcept { return orders_; }
	const INT8& orders() const noexcept { return orders_; }
	INT8& attitude() noexcept { return attitude_; }
	const INT8& attitude() const noexcept { return attitude_; }
	INT8& underEscort() noexcept { return underEscort_; }
	const INT8& underEscort() const noexcept { return underEscort_; }
	INT8& bypassToGreen() noexcept { return bypassToGreen_; }
	const INT8& bypassToGreen() const noexcept { return bypassToGreen_; }
	INT8& hunting() noexcept { return hunting_; }
	const INT8& hunting() const noexcept { return hunting_; }
	INT8& mobility() noexcept { return mobility_; }
	const INT8& mobility() const noexcept { return mobility_; }
	INT8& realtimeCombat() noexcept { return realtimeCombat_; }
	const INT8& realtimeCombat() const noexcept { return realtimeCombat_; }
	INT8& flags() noexcept { return flags_; }
	const INT8& flags() const noexcept { return flags_; }

	bool hasFlag(INT8 flag) const noexcept { return (flags_ & flag) != 0; }
	void setFlag(INT8 flag) noexcept { flags_ = static_cast<INT8>(flags_ | flag); }
	void clearFlag(INT8 flag) noexcept { flags_ = static_cast<INT8>(flags_ & ~flag); }
	void reset() noexcept;

private:
	INT8 alertStatus_ = 0;
	INT8 neutral_ = 0;
	INT8 newSituation_ = 0;
	INT8 orders_ = 0;
	INT8 attitude_ = 0;
	INT8 underEscort_ = 0;
	INT8 bypassToGreen_ = 0;
	INT8 hunting_ = 0;
	INT8 mobility_ = 0;
	INT8 realtimeCombat_ = 0;
	INT8 flags_ = 0;
};

// Canonical radio/call exchange state used by human and creature AI.
class SoldierAiCommunicationComponent
{
public:
	UINT8& lastMercToRadio() noexcept { return lastMercToRadio_; }
	const UINT8& lastMercToRadio() const noexcept { return lastMercToRadio_; }
	UINT8& lastCall() noexcept { return lastCall_; }
	const UINT8& lastCall() const noexcept { return lastCall_; }
	SoldierID& caller() noexcept { return caller_; }
	const SoldierID& caller() const noexcept { return caller_; }
	INT32& callerGrid() noexcept { return callerGrid_; }
	const INT32& callerGrid() const noexcept { return callerGrid_; }
	UINT8& callPriority() noexcept { return callPriority_; }
	const UINT8& callPriority() const noexcept { return callPriority_; }
	INT8& callActedUpon() noexcept { return callActedUpon_; }
	const INT8& callActedUpon() const noexcept { return callActedUpon_; }

	bool hasUnansweredCall() const noexcept { return callPriority_ != 0 && callActedUpon_ == 0; }
	void reset() noexcept;

private:
	UINT8 lastMercToRadio_ = 0;
	UINT8 lastCall_ = 0;
	SoldierID caller_{ static_cast<UINT16>(0) };
	INT32 callerGrid_ = 0;
	UINT8 callPriority_ = 0;
	INT8 callActedUpon_ = 0;
};

// Canonical personal and tactical morale state. Strategic/tactical/team
// modifiers remain distinct because established rules update them separately.
class SoldierMoraleComponent
{
public:
	INT8& morale() noexcept { return morale_; }
	const INT8& morale() const noexcept { return morale_; }
	INT8& teamModifier() noexcept { return teamModifier_; }
	const INT8& teamModifier() const noexcept { return teamModifier_; }
	INT8& tacticalModifier() noexcept { return tacticalModifier_; }
	const INT8& tacticalModifier() const noexcept { return tacticalModifier_; }
	INT8& strategicModifier() noexcept { return strategicModifier_; }
	const INT8& strategicModifier() const noexcept { return strategicModifier_; }
	INT8& aiMorale() noexcept { return aiMorale_; }
	const INT8& aiMorale() const noexcept { return aiMorale_; }
	INT8& frenzied() noexcept { return frenzied_; }
	const INT8& frenzied() const noexcept { return frenzied_; }

	bool isFrenzied() const noexcept { return frenzied_ != 0; }
	void reset() noexcept;

private:
	INT8 morale_ = 0;
	INT8 teamModifier_ = 0;
	INT8 tacticalModifier_ = 0;
	INT8 strategicModifier_ = 0;
	INT8 aiMorale_ = 0;
	INT8 frenzied_ = 0;
};

// Canonical skill execution and persistence state. Repeated mechanical checks,
// the AI's selected skill, trait counters, heterogeneous cooldowns, and focus
// targeting share one reset boundary without absorbing permanent statistics or
// rule definitions.
class SoldierSkillStateComponent
{
public:
	using Counters = UINT16[SOLDIER_COUNTER_MAX];
	using Cooldowns = UINT32[SOLDIER_COOLDOWN_MAX];
	static constexpr INT8 MaximumCheckAttempts = 127;

	INT8& lastCheckReason() noexcept { return lastCheckReason_; }
	const INT8& lastCheckReason() const noexcept { return lastCheckReason_; }
	INT8& checkAttempts() noexcept { return checkAttempts_; }
	const INT8& checkAttempts() const noexcept { return checkAttempts_; }
	INT32& checkGrid() noexcept { return checkGrid_; }
	const INT32& checkGrid() const noexcept { return checkGrid_; }
	UINT8& selectedAiSkill() noexcept { return selectedAiSkill_; }
	const UINT8& selectedAiSkill() const noexcept { return selectedAiSkill_; }
	UINT16& counter(UINT8 index) noexcept { return counters_[index]; }
	const UINT16& counter(UINT8 index) const noexcept { return counters_[index]; }
	UINT32& cooldown(UINT8 index) noexcept { return cooldowns_[index]; }
	const UINT32& cooldown(UINT8 index) const noexcept { return cooldowns_[index]; }
	INT32& focusGrid() noexcept { return focusGrid_; }
	const INT32& focusGrid() const noexcept { return focusGrid_; }

	bool isRepeatedCheck(INT8 reason, INT32 grid) const noexcept
	{
		return lastCheckReason_ == reason && checkGrid_ == grid;
	}
	bool hasCounter(UINT8 index) const noexcept { return counters_[index] != 0; }
	bool hasCooldown(UINT8 index) const noexcept { return cooldowns_[index] != 0; }
	void beginCheck(INT8 reason, INT32 grid) noexcept
	{
		lastCheckReason_ = reason;
		checkAttempts_ = 1;
		checkGrid_ = grid;
	}
	void recordCheckAttempt() noexcept
	{
		if (checkAttempts_ < MaximumCheckAttempts)
		{
			++checkAttempts_;
		}
	}
	void clearCounter(UINT8 index) noexcept { counters_[index] = 0; }
	void decrementCooldown(UINT8 index) noexcept
	{
		if (cooldowns_[index] > 0)
		{
			--cooldowns_[index];
		}
	}
	void clearCooldown(UINT8 index) noexcept { cooldowns_[index] = 0; }
	void ageTurnCounters() noexcept;
	void focusOn(INT32 grid) noexcept { focusGrid_ = grid; }
	void clearFocus() noexcept { focusGrid_ = -1; }
	void reset() noexcept;

private:
	INT8 lastCheckReason_ = 0;
	INT8 checkAttempts_ = 0;
	INT32 checkGrid_ = 0;
	UINT8 selectedAiSkill_ = 0;
	Counters counters_{};
	Cooldowns cooldowns_{};
	INT32 focusGrid_ = 0;
};

// Canonical ongoing condition state outside core health/breath vitals.
// Temporary stat effects, nutrition and starvation harm, disease progress, and
// acquired disabilities share one reset boundary without importing disease
// rule definitions or content records.
class SoldierConditionComponent
{
public:
	using DiseasePoints = INT16[NUM_DISEASES];
	using DiseaseFlags = UINT8[NUM_DISEASES];
	static constexpr UINT8 DisabilityBitCount = 32;

	INT16& extraStrength() noexcept { return extraStrength_; }
	const INT16& extraStrength() const noexcept { return extraStrength_; }
	INT16& extraDexterity() noexcept { return extraDexterity_; }
	const INT16& extraDexterity() const noexcept { return extraDexterity_; }
	INT16& extraAgility() noexcept { return extraAgility_; }
	const INT16& extraAgility() const noexcept { return extraAgility_; }
	INT16& extraWisdom() noexcept { return extraWisdom_; }
	const INT16& extraWisdom() const noexcept { return extraWisdom_; }
	INT8& extraExperienceLevel() noexcept { return extraExperienceLevel_; }
	const INT8& extraExperienceLevel() const noexcept { return extraExperienceLevel_; }
	INT32& foodLevel() noexcept { return foodLevel_; }
	const INT32& foodLevel() const noexcept { return foodLevel_; }
	INT32& drinkLevel() noexcept { return drinkLevel_; }
	const INT32& drinkLevel() const noexcept { return drinkLevel_; }
	UINT8& starvationHealthDamage() noexcept { return starvationHealthDamage_; }
	const UINT8& starvationHealthDamage() const noexcept { return starvationHealthDamage_; }
	UINT8& starvationStrengthDamage() noexcept { return starvationStrengthDamage_; }
	const UINT8& starvationStrengthDamage() const noexcept { return starvationStrengthDamage_; }
	INT16& diseasePoints(UINT8 index) noexcept { return diseasePoints_[index]; }
	const INT16& diseasePoints(UINT8 index) const noexcept { return diseasePoints_[index]; }
	UINT8& diseaseFlags(UINT8 index) noexcept { return diseaseFlags_[index]; }
	const UINT8& diseaseFlags(UINT8 index) const noexcept { return diseaseFlags_[index]; }
	UINT32& disabilityFlags() noexcept { return disabilityFlags_; }
	const UINT32& disabilityFlags() const noexcept { return disabilityFlags_; }
	UINT8& gasHitFlags() noexcept { return gasHitFlags_; }
	const UINT8& gasHitFlags() const noexcept { return gasHitFlags_; }

	bool hasExtraStats() const noexcept;
	bool hasStarvationDamage() const noexcept;
	bool infected(UINT8 index) const noexcept { return diseasePoints_[index] > 0; }
	bool hasDiseaseFlag(UINT8 index, UINT8 flag) const noexcept
	{
		return (diseaseFlags_[index] & flag) != 0;
	}
	bool hasDisability(UINT8 disability) const noexcept;
	void markDiseaseFlag(UINT8 index, UINT8 flag) noexcept
	{
		diseaseFlags_[index] |= flag;
	}
	void clearDiseaseFlags(UINT8 index, UINT8 flags) noexcept
	{
		diseaseFlags_[index] &= static_cast<UINT8>(~flags);
	}
	void addDisability(UINT8 disability) noexcept;
	void clearExtraStats() noexcept;
	void reset() noexcept;

private:
	INT16 extraStrength_ = 0;
	INT16 extraDexterity_ = 0;
	INT16 extraAgility_ = 0;
	INT16 extraWisdom_ = 0;
	INT8 extraExperienceLevel_ = 0;
	INT32 foodLevel_ = 0;
	INT32 drinkLevel_ = 0;
	UINT8 starvationHealthDamage_ = 0;
	UINT8 starvationStrengthDamage_ = 0;
	DiseasePoints diseasePoints_{};
	DiseaseFlags diseaseFlags_{};
	UINT32 disabilityFlags_ = 0;
	UINT8 gasHitFlags_ = 0;
};

// Canonical persistent drug and alcohol state. Fixed effect arrays retain the
// established save capacity while named operations coordinate effect merging,
// temporary personality/disability lifetimes, and alcohol metabolism.
class SoldierDrugStateComponent
{
public:
	static constexpr UINT8 EffectCapacity = DRUG_EFFECT_MAX;
	using EffectDurations = UINT16[EffectCapacity];
	using EffectMagnitudes = INT16[EffectCapacity];

	UINT16& duration(UINT8 effect) noexcept { return durations_[effect]; }
	const UINT16& duration(UINT8 effect) const noexcept { return durations_[effect]; }
	INT16& magnitude(UINT8 effect) noexcept { return magnitudes_[effect]; }
	const INT16& magnitude(UINT8 effect) const noexcept { return magnitudes_[effect]; }
	UINT8& temporaryPersonality() noexcept { return temporaryPersonality_; }
	const UINT8& temporaryPersonality() const noexcept { return temporaryPersonality_; }
	UINT16& temporaryPersonalityDuration() noexcept
	{
		return temporaryPersonalityDuration_;
	}
	const UINT16& temporaryPersonalityDuration() const noexcept
	{
		return temporaryPersonalityDuration_;
	}
	UINT8& temporaryDisability() noexcept { return temporaryDisability_; }
	const UINT8& temporaryDisability() const noexcept { return temporaryDisability_; }
	UINT16& temporaryDisabilityDuration() noexcept
	{
		return temporaryDisabilityDuration_;
	}
	const UINT16& temporaryDisabilityDuration() const noexcept
	{
		return temporaryDisabilityDuration_;
	}
	FLOAT& alcoholLevel() noexcept { return alcoholLevel_; }
	const FLOAT& alcoholLevel() const noexcept { return alcoholLevel_; }

	bool hasEffect(UINT8 effect) const noexcept
	{
		return effect < EffectCapacity && durations_[effect] != 0;
	}
	bool hasTemporaryPersonality(UINT8 personality) const noexcept
	{
		return temporaryPersonality_ == personality;
	}
	bool hasTemporaryDisability(UINT8 disability) const noexcept
	{
		return temporaryDisability_ == disability;
	}
	bool hasAlcohol() const noexcept { return alcoholLevel_ > 0.0f; }

	void mergeEffect(
		UINT8 effect, UINT16 duration, INT16 magnitude, FLOAT scale) noexcept;
	void applyTemporaryPersonality(
		UINT8 personality, UINT16 duration) noexcept;
	void applyTemporaryDisability(
		UINT8 disability, UINT16 duration) noexcept;
	bool ageTurn() noexcept;
	void addAlcohol(FLOAT amount) noexcept { alcoholLevel_ += amount; }
	bool metabolizeAlcohol(FLOAT amount) noexcept;
	void reset() noexcept;

private:
	EffectDurations durations_{};
	EffectMagnitudes magnitudes_{};
	UINT8 temporaryPersonality_ = 0;
	UINT16 temporaryPersonalityDuration_ = 0;
	UINT8 temporaryDisability_ = 0;
	UINT16 temporaryDisabilityDuration_ = 0;
	FLOAT alcoholLevel_ = 0.0f;
};

// Canonical timestamps for persistent-stat changes. Gameplay records changes
// through the stat identity; tactical and strategic UI share the same
// wrap-safe recent-change query instead of duplicating clock arithmetic.
class SoldierStatProgressComponent
{
public:
	enum class Stat : UINT8
	{
		Level,
		Health,
		Strength,
		Dexterity,
		Agility,
		Wisdom,
		Leadership,
		Marksmanship,
		Explosives,
		Medical,
		Mechanical,
		Count,
	};

	static constexpr UINT8 StatCount = static_cast<UINT8>(Stat::Count);

	UINT32& changedAt(Stat stat) noexcept { return changeTimes_[index(stat)]; }
	const UINT32& changedAt(Stat stat) const noexcept { return changeTimes_[index(stat)]; }
	UINT16& increaseMask() noexcept { return increaseMask_; }
	const UINT16& increaseMask() const noexcept { return increaseMask_; }

	bool hasChange(Stat stat) const noexcept { return changedAt(stat) != 0; }
	bool changedRecently(Stat stat, UINT32 now, UINT32 duration) const noexcept
	{
		const UINT32 changeTime = changedAt(stat);
		return changeTime != 0 && now - changeTime < duration;
	}
	void recordChange(Stat stat, UINT32 now) noexcept { changedAt(stat) = now; }
	void clear(Stat stat) noexcept { changedAt(stat) = 0; }
	bool increased(UINT16 mask) const noexcept { return (increaseMask_ & mask) != 0; }
	void markIncreased(UINT16 mask) noexcept { increaseMask_ |= mask; }
	void clearIncreased(UINT16 mask) noexcept { increaseMask_ &= ~mask; }
	void reset() noexcept;

private:
	static constexpr UINT8 index(Stat stat) noexcept
	{
		return static_cast<UINT8>(stat);
	}

	UINT32 changeTimes_[StatCount] = {};
	UINT16 increaseMask_ = 0;
};

// Canonical soldier-local countdown timers and their AI/reload delay
// configuration. Gameplay starts, observes, and clears timers by purpose while
// the platform timer loop retains mutable access for elapsed-time updates.
class SoldierTimingComponent
{
public:
	enum class Timer : UINT8
	{
		AnimationUpdate,
		DamageDisplay,
		Reload,
		LocatorFlash,
		Ai,
		Fade,
		PanelAnimation,
		LocatorBlink,
		PortraitFlash,
		NextTile,
		Count,
	};

	static constexpr UINT8 TimerCount = static_cast<UINT8>(Timer::Count);

	INT32& counter(Timer timer) noexcept { return counters_[index(timer)]; }
	const INT32& counter(Timer timer) const noexcept { return counters_[index(timer)]; }
	UINT32& aiDelay() noexcept { return aiDelay_; }
	const UINT32& aiDelay() const noexcept { return aiDelay_; }
	INT16& reloadDelay() noexcept { return reloadDelay_; }
	const INT16& reloadDelay() const noexcept { return reloadDelay_; }

	bool active(Timer timer) const noexcept { return counter(timer) != 0; }
	bool elapsed(Timer timer) const noexcept { return counter(timer) == 0; }
	void start(Timer timer, INT32 duration) noexcept { counter(timer) = duration; }
	void clear(Timer timer) noexcept { counter(timer) = 0; }
	void reset() noexcept;

private:
	static constexpr UINT8 index(Timer timer) noexcept
	{
		return static_cast<UINT8>(timer);
	}

	INT32 counters_[TimerCount] = {};
	UINT32 aiDelay_ = 0;
	INT16 reloadDelay_ = 0;
};

// Canonical state for work that spans tactical turns. The retained context
// grid also carries the established return location while an intel assignment
// temporarily removes a soldier from the tactical world; it deliberately
// remains independent of the soldier's current position.
class SoldierLongActionComponent
{
public:
	INT16& remainingActionPoints() noexcept { return remainingActionPoints_; }
	const INT16& remainingActionPoints() const noexcept { return remainingActionPoints_; }
	INT32& contextGrid() noexcept { return contextGrid_; }
	const INT32& contextGrid() const noexcept { return contextGrid_; }
	UINT8& action() noexcept { return action_; }
	const UINT8& action() const noexcept { return action_; }

	bool active() const noexcept { return action_ != 0; }
	void begin(UINT8 action, INT32 contextGrid, INT16 actionPoints) noexcept;
	void rememberContextGrid(INT32 contextGrid) noexcept { contextGrid_ = contextGrid; }
	void completeCost() noexcept { remainingActionPoints_ = 0; }
	void consumeActionPoints(INT16 actionPoints) noexcept;
	void clear() noexcept;
	void reset() noexcept;

private:
	static constexpr INT32 NoContextGrid = -1;

	INT16 remainingActionPoints_ = 0;
	INT32 contextGrid_ = NoContextGrid;
	UINT8 action_ = 0;
};

// Canonical state for direct world interactions. Non-profile merchant identity,
// mutually exclusive person/corpse/structure dragging, and the reciprocal chat
// partner share one reset boundary without owning the referenced entities.
class SoldierInteractionComponent
{
public:
	INT8& nonNpcTraderId() noexcept { return nonNpcTraderId_; }
	const INT8& nonNpcTraderId() const noexcept { return nonNpcTraderId_; }
	SoldierID& draggedPerson() noexcept { return draggedPerson_; }
	const SoldierID& draggedPerson() const noexcept { return draggedPerson_; }
	INT16& draggedCorpse() noexcept { return draggedCorpse_; }
	const INT16& draggedCorpse() const noexcept { return draggedCorpse_; }
	SoldierID& chatPartner() noexcept { return chatPartner_; }
	const SoldierID& chatPartner() const noexcept { return chatPartner_; }
	INT32& draggedStructureGrid() noexcept { return draggedStructureGrid_; }
	const INT32& draggedStructureGrid() const noexcept { return draggedStructureGrid_; }

	bool isNonNpcTrader() const noexcept { return nonNpcTraderId_ > 0; }
	bool draggingPerson() const noexcept { return draggedPerson_ != NOBODY; }
	bool draggingCorpse() const noexcept { return draggedCorpse_ >= 0; }
	bool draggingStructure() const noexcept { return draggedStructureGrid_ >= 0; }
	bool dragging() const noexcept
	{
		return draggingPerson() || draggingCorpse() || draggingStructure();
	}
	bool chatting() const noexcept { return chatPartner_ != NOBODY; }

	void dragPerson(SoldierID soldier) noexcept;
	void dragCorpse(INT16 corpse) noexcept;
	void dragStructure(INT32 grid) noexcept;
	void copyDragFrom(const SoldierInteractionComponent& source) noexcept;
	void clearDrag() noexcept;
	void beginChatWith(SoldierID soldier) noexcept { chatPartner_ = soldier; }
	void endChat() noexcept { chatPartner_ = NOBODY; }
	void reset() noexcept;

private:
	static constexpr INT32 NoGrid = -1;

	INT8 nonNpcTraderId_ = 0;
	SoldierID draggedPerson_ = NOBODY;
	INT16 draggedCorpse_ = -1;
	SoldierID chatPartner_ = NOBODY;
	INT32 draggedStructureGrid_ = NoGrid;
};

// Canonical persisted state for a tactical action that has been selected but
// not yet completed. The numbered payloads deliberately remain generic because
// their established meaning depends on the action kind. Runtime-only target
// incarnation, path-search, launcher, and callback scratch remain outside this
// component in SoldierPendingActionRuntimeState.
class SoldierPendingActionComponent
{
public:
	static constexpr UINT8 NoAction = 255;

	UINT8& action() noexcept { return action_; }
	const UINT8& action() const noexcept { return action_; }
	UINT8& animationCount() noexcept { return animationCount_; }
	const UINT8& animationCount() const noexcept { return animationCount_; }
	UINT32& primaryData() noexcept { return primaryData_; }
	const UINT32& primaryData() const noexcept { return primaryData_; }
	INT32& secondaryData() noexcept { return secondaryData_; }
	const INT32& secondaryData() const noexcept { return secondaryData_; }
	INT8& tertiaryData() noexcept { return tertiaryData_; }
	const INT8& tertiaryData() const noexcept { return tertiaryData_; }
	INT8& doorHandleCode() noexcept { return doorHandleCode_; }
	const INT8& doorHandleCode() const noexcept { return doorHandleCode_; }
	UINT32& quaternaryData() noexcept { return quaternaryData_; }
	const UINT32& quaternaryData() const noexcept { return quaternaryData_; }
	INT32& nextSpecialData() noexcept { return nextSpecialData_; }
	const INT32& nextSpecialData() const noexcept { return nextSpecialData_; }
	UINT8& interruptionMarker() noexcept { return interruptionMarker_; }
	const UINT8& interruptionMarker() const noexcept { return interruptionMarker_; }
	INT8& inventorySlot() noexcept { return inventorySlot_; }
	const INT8& inventorySlot() const noexcept { return inventorySlot_; }

	bool active() const noexcept { return action_ != NoAction; }
	void begin(UINT8 action) noexcept
	{
		action_ = action;
		animationCount_ = 0;
	}
	void clearAction() noexcept { action_ = NoAction; }
	void clearPayload() noexcept;
	void resetAnimationCount() noexcept { animationCount_ = 0; }
	void recordAnimationTransition() noexcept;
	void reset() noexcept;

private:
	UINT8 action_ = NoAction;
	UINT8 animationCount_ = 0;
	UINT32 primaryData_ = 0;
	INT32 secondaryData_ = 0;
	INT8 tertiaryData_ = 0;
	INT8 doorHandleCode_ = 0;
	UINT32 quaternaryData_ = 0;
	INT32 nextSpecialData_ = 0;
	UINT8 interruptionMarker_ = 0;
	INT8 inventorySlot_ = 0;
};

// Canonical tactical action-point budget. The current amount and the turn-start
// snapshot form one lifecycle: turn setup records them together, while network
// reconciliation may still update only the authoritative current amount.
class SoldierActionPointComponent
{
public:
	INT16& current() noexcept { return current_; }
	const INT16& current() const noexcept { return current_; }
	INT16& initial() noexcept { return initial_; }
	const INT16& initial() const noexcept { return initial_; }

	bool hasAny() const noexcept { return current_ > 0; }
	void beginTurn(INT16 points) noexcept;
	void snapshotTurnStart() noexcept { initial_ = current_; }
	void clear() noexcept;
	void reset() noexcept;

private:
	INT16 current_ = 0;
	INT16 initial_ = 0;
};

// Canonical tactical and strategic collapse lifecycle. Tactical
// incapacitation, breath-collapse staging, recovery duration, sleep-drug
// duration, and strategic fatigue collapse share one explicit reset boundary
// without conflating them with health or the action-point budget.
class SoldierCollapseComponent
{
public:
	INT8& tactical() noexcept { return tactical_; }
	const INT8& tactical() const noexcept { return tactical_; }
	INT8& breathTriggered() noexcept { return breathTriggered_; }
	const INT8& breathTriggered() const noexcept { return breathTriggered_; }
	INT8& turns() noexcept { return turns_; }
	const INT8& turns() const noexcept { return turns_; }
	INT8& sleepDrugCounter() noexcept { return sleepDrugCounter_; }
	const INT8& sleepDrugCounter() const noexcept { return sleepDrugCounter_; }
	BOOLEAN& fatigue() noexcept { return fatigue_; }
	const BOOLEAN& fatigue() const noexcept { return fatigue_; }

	bool collapsed() const noexcept { return tactical_ != FALSE; }
	bool breathCollapsed() const noexcept { return breathTriggered_ != FALSE; }
	bool fatigueCollapsed() const noexcept { return fatigue_ != FALSE; }
	void collapse() noexcept { tactical_ = TRUE; }
	void clearTactical() noexcept { tactical_ = FALSE; }
	void recover() noexcept;
	void markBreathCollapse() noexcept { breathTriggered_ = TRUE; }
	void clearBreathCollapse() noexcept { breathTriggered_ = FALSE; }
	void markFatigueCollapse() noexcept { fatigue_ = TRUE; }
	void clearFatigueCollapse() noexcept { fatigue_ = FALSE; }
	void reset() noexcept;

private:
	INT8 tactical_ = FALSE;
	INT8 breathTriggered_ = FALSE;
	INT8 turns_ = 0;
	INT8 sleepDrugCounter_ = 0;
	BOOLEAN fatigue_ = FALSE;
};

// Canonical sensory state and short-lived perception effects. View range,
// personal noise and smell memory, X-ray source/lifetime, blindness, and
// deafness share one reset boundary without owning opponent knowledge or
// presentation visibility.
class SoldierPerceptionComponent
{
public:
	UINT8& movementNoiseDirections() noexcept { return movementNoiseDirections_; }
	const UINT8& movementNoiseDirections() const noexcept { return movementNoiseDirections_; }
	UINT8& viewRange() noexcept { return viewRange_; }
	const UINT8& viewRange() const noexcept { return viewRange_; }
	INT8& blindnessTurns() noexcept { return blindnessTurns_; }
	const INT8& blindnessTurns() const noexcept { return blindnessTurns_; }
	INT8& heardNoiseLevel() noexcept { return heardNoiseLevel_; }
	const INT8& heardNoiseLevel() const noexcept { return heardNoiseLevel_; }
	UINT32& xrayActivatedAt() noexcept { return xrayActivatedAt_; }
	const UINT32& xrayActivatedAt() const noexcept { return xrayActivatedAt_; }
	INT8& deafnessTurns() noexcept { return deafnessTurns_; }
	const INT8& deafnessTurns() const noexcept { return deafnessTurns_; }
	INT32& noiseGrid() noexcept { return noiseGrid_; }
	const INT32& noiseGrid() const noexcept { return noiseGrid_; }
	UINT8& noiseVolume() noexcept { return noiseVolume_; }
	const UINT8& noiseVolume() const noexcept { return noiseVolume_; }
	SoldierID& xraySource() noexcept { return xraySource_; }
	const SoldierID& xraySource() const noexcept { return xraySource_; }
	INT8& normalSmell() noexcept { return normalSmell_; }
	const INT8& normalSmell() const noexcept { return normalSmell_; }
	INT8& monsterSmell() noexcept { return monsterSmell_; }
	const INT8& monsterSmell() const noexcept { return monsterSmell_; }

	bool isBlinded() const noexcept { return blindnessTurns_ > 0; }
	bool isDeafened() const noexcept { return deafnessTurns_ > 0; }
	bool xrayActive() const noexcept { return xrayActivatedAt_ != 0; }
	bool hasHeardMovementFrom(UINT8 direction) const noexcept;
	void rememberMovementFrom(UINT8 direction) noexcept;
	void clearMovementDirections() noexcept { movementNoiseDirections_ = 0; }
	void addBlindness(INT16 turns) noexcept
	{
		blindnessTurns_ = static_cast<INT8>(blindnessTurns_ + turns);
	}
	void setBlindness(INT8 turns) noexcept { blindnessTurns_ = turns; }
	bool extendBlindnessToAtLeast(INT32 turns) noexcept;
	bool ageBlindness() noexcept;
	void setDeafness(INT8 turns) noexcept { deafnessTurns_ = turns; }
	void halveDeafness() noexcept { deafnessTurns_ /= 2; }
	void ageDeafness() noexcept;
	void activateXrayAt(UINT32 worldSeconds) noexcept { xrayActivatedAt_ = worldSeconds; }
	void deactivateXray() noexcept { xrayActivatedAt_ = 0; }
	void clearNoise() noexcept
	{
		noiseGrid_ = -1;
		noiseVolume_ = 0;
	}
	void reset() noexcept;

private:
	UINT8 movementNoiseDirections_ = 0;
	UINT8 viewRange_ = 0;
	INT8 blindnessTurns_ = 0;
	INT8 heardNoiseLevel_ = 0;
	UINT32 xrayActivatedAt_ = 0;
	INT8 deafnessTurns_ = 0;
	INT32 noiseGrid_ = 0;
	UINT8 noiseVolume_ = 0;
	SoldierID xraySource_{ static_cast<UINT16>(0) };
	INT8 normalSmell_ = 0;
	INT8 monsterSmell_ = 0;
};

// Canonical tactical awareness state. This owns whether the player currently
// knows where the soldier is, the last visibility consumed by rendering, the
// local opponent-knowledge table and counts, and the movement distance used to
// age stale knowledge. Sensory capability remains in SoldierPerceptionComponent.
class SoldierAwarenessComponent
{
public:
	using OpponentKnowledge = INT8[MAX_NUM_SOLDIERS];

	INT8& visibility() noexcept { return visibility_; }
	const INT8& visibility() const noexcept { return visibility_; }
	INT8& lastRenderedVisibility() noexcept { return lastRenderedVisibility_; }
	const INT8& lastRenderedVisibility() const noexcept { return lastRenderedVisibility_; }
	INT8& newOpponentCount() noexcept { return newOpponentCount_; }
	const INT8& newOpponentCount() const noexcept { return newOpponentCount_; }
	UINT8& tilesSinceForget() noexcept { return tilesSinceForget_; }
	const UINT8& tilesSinceForget() const noexcept { return tilesSinceForget_; }
	OpponentKnowledge& opponentKnowledge() noexcept { return opponentKnowledge_; }
	const OpponentKnowledge& opponentKnowledge() const noexcept { return opponentKnowledge_; }
	INT8& opponentCount() noexcept { return opponentCount_; }
	const INT8& opponentCount() const noexcept { return opponentCount_; }

	bool visibleNow() const noexcept { return visibility_ == TRUE; }
	bool fullyHidden() const noexcept { return visibility_ == -1; }
	bool fadingOut() const noexcept { return visibility_ == -2; }
	bool locationKnown() const noexcept { return visibility_ >= 0; }
	bool renderVisibilityChanged() const noexcept
	{
		return visibility_ != lastRenderedVisibility_;
	}
	bool hasNewOpponents() const noexcept { return newOpponentCount_ != 0; }
	void markVisible() noexcept { visibility_ = TRUE; }
	void markHidden() noexcept { visibility_ = -1; }
	void markIndeterminate() noexcept { visibility_ = 0; }
	void beginFadeOut() noexcept { visibility_ = -2; }
	void syncRenderedVisibility() noexcept { lastRenderedVisibility_ = visibility_; }
	void setVisibilityAndRendered(INT8 visibility) noexcept
	{
		visibility_ = visibility;
		lastRenderedVisibility_ = visibility;
	}
	void recordNewOpponent() noexcept
	{
		if (newOpponentCount_ < 127)
		{
			++newOpponentCount_;
		}
	}
	void clearNewOpponents() noexcept { newOpponentCount_ = 0; }
	void recordTileForMemory() noexcept
	{
		if (tilesSinceForget_ < 255)
		{
			++tilesSinceForget_;
		}
	}
	void resetForgetDistance() noexcept { tilesSinceForget_ = 0; }
	void reset() noexcept;

private:
	INT8 visibility_ = 0;
	INT8 lastRenderedVisibility_ = 0;
	INT8 newOpponentCount_ = 0;
	UINT8 tilesSinceForget_ = 0;
	OpponentKnowledge opponentKnowledge_{};
	INT8 opponentCount_ = 0;
};

// Canonical personal camouflage state. Applied kit and worn-equipment values
// remain separate for each terrain family while totals and clamping live in one
// place shared by sight calculations and presentation.
class SoldierCamouflageComponent
{
public:
	enum class Terrain : UINT8
	{
		Jungle,
		Urban,
		Desert,
		Snow,
	};

	INT8& jungleApplied() noexcept { return jungleApplied_; }
	const INT8& jungleApplied() const noexcept { return jungleApplied_; }
	INT8& jungleWorn() noexcept { return jungleWorn_; }
	const INT8& jungleWorn() const noexcept { return jungleWorn_; }
	INT8& urbanApplied() noexcept { return urbanApplied_; }
	const INT8& urbanApplied() const noexcept { return urbanApplied_; }
	INT8& urbanWorn() noexcept { return urbanWorn_; }
	const INT8& urbanWorn() const noexcept { return urbanWorn_; }
	INT8& desertApplied() noexcept { return desertApplied_; }
	const INT8& desertApplied() const noexcept { return desertApplied_; }
	INT8& desertWorn() noexcept { return desertWorn_; }
	const INT8& desertWorn() const noexcept { return desertWorn_; }
	INT8& snowApplied() noexcept { return snowApplied_; }
	const INT8& snowApplied() const noexcept { return snowApplied_; }
	INT8& snowWorn() noexcept { return snowWorn_; }
	const INT8& snowWorn() const noexcept { return snowWorn_; }

	INT8 total(Terrain terrain) const noexcept;
	INT8 strongestTotal() const noexcept;
	INT16 appliedTotal() const noexcept;
	void reset() noexcept;

private:
	INT8 jungleApplied_ = 0;
	INT8 jungleWorn_ = 0;
	INT8 urbanApplied_ = 0;
	INT8 urbanWorn_ = 0;
	INT8 desertApplied_ = 0;
	INT8 desertWorn_ = 0;
	INT8 snowApplied_ = 0;
	INT8 snowWorn_ = 0;
};

// Canonical strategic employment state. Contract timing, mercenary
// classification, deposits, insurance, renewal decisions, price-change
// acknowledgement, and re-signing eligibility remain distinct values but
// share one lifecycle owner.
class SoldierEmploymentComponent
{
public:
	INT32& endTime() noexcept { return endTime_; }
	const INT32& endTime() const noexcept { return endTime_; }
	INT32& startTime() noexcept { return startTime_; }
	const INT32& startTime() const noexcept { return startTime_; }
	INT32& totalLength() noexcept { return totalLength_; }
	const INT32& totalLength() const noexcept { return totalLength_; }
	UINT8& mercenaryType() noexcept { return mercenaryType_; }
	const UINT8& mercenaryType() const noexcept { return mercenaryType_; }
	UINT16& medicalDeposit() noexcept { return medicalDeposit_; }
	const UINT16& medicalDeposit() const noexcept { return medicalDeposit_; }
	UINT16& lifeInsurance() noexcept { return lifeInsurance_; }
	const UINT16& lifeInsurance() const noexcept { return lifeInsurance_; }
	INT32& insuranceStartDay() noexcept { return insuranceStartDay_; }
	const INT32& insuranceStartDay() const noexcept { return insuranceStartDay_; }
	INT32& insuranceLengthDays() noexcept { return insuranceLengthDays_; }
	const INT32& insuranceLengthDays() const noexcept { return insuranceLengthDays_; }
	UINT32& lastContractUpdateTime() noexcept { return lastContractUpdateTime_; }
	const UINT32& lastContractUpdateTime() const noexcept { return lastContractUpdateTime_; }
	INT8& lastContractType() noexcept { return lastContractType_; }
	const INT8& lastContractType() const noexcept { return lastContractType_; }
	UINT8& justFired() noexcept { return justFired_; }
	const UINT8& justFired() const noexcept { return justFired_; }
	UINT8& renewalQuoteCode() noexcept { return renewalQuoteCode_; }
	const UINT8& renewalQuoteCode() const noexcept { return renewalQuoteCode_; }
	INT32& timeCanSignElsewhere() noexcept { return timeCanSignElsewhere_; }
	const INT32& timeCanSignElsewhere() const noexcept { return timeCanSignElsewhere_; }
	INT8& hospitalPriceModifier() noexcept { return hospitalPriceModifier_; }
	const INT8& hospitalPriceModifier() const noexcept { return hospitalPriceModifier_; }
	UINT32& insuranceStartTime() noexcept { return insuranceStartTime_; }
	const UINT32& insuranceStartTime() const noexcept { return insuranceStartTime_; }
	BOOLEAN& contractPriceIncreasedState() noexcept { return contractPriceIncreased_; }
	const BOOLEAN& contractPriceIncreasedState() const noexcept { return contractPriceIncreased_; }
	BOOLEAN& signedAnotherContractState() noexcept { return signedAnotherContract_; }
	const BOOLEAN& signedAnotherContractState() const noexcept { return signedAnotherContract_; }

	bool isMercenaryType(UINT8 type) const noexcept { return mercenaryType_ == type; }
	bool hasMedicalDeposit() const noexcept { return medicalDeposit_ != 0; }
	bool hasLifeInsurance() const noexcept { return lifeInsurance_ != 0; }
	bool wasJustFired() const noexcept { return justFired_ != 0; }
	bool hasContractPriceIncrease() const noexcept { return contractPriceIncreased_ != FALSE; }
	bool hasSignedAnotherContract() const noexcept { return signedAnotherContract_ != FALSE; }
	void markContractPriceIncreased() noexcept { contractPriceIncreased_ = TRUE; }
	void acknowledgeContractPriceIncrease() noexcept { contractPriceIncreased_ = FALSE; }
	void markSignedAnotherContract() noexcept { signedAnotherContract_ = TRUE; }
	void clearSignedAnotherContract() noexcept { signedAnotherContract_ = FALSE; }
	void reset() noexcept;

private:
	INT32 endTime_ = 0;
	INT32 startTime_ = 0;
	INT32 totalLength_ = 0;
	UINT8 mercenaryType_ = 0;
	UINT16 medicalDeposit_ = 0;
	UINT16 lifeInsurance_ = 0;
	INT32 insuranceStartDay_ = 0;
	INT32 insuranceLengthDays_ = 0;
	UINT32 lastContractUpdateTime_ = 0;
	INT8 lastContractType_ = 0;
	UINT8 justFired_ = 0;
	UINT8 renewalQuoteCode_ = 0;
	INT32 timeCanSignElsewhere_ = 0;
	INT8 hospitalPriceModifier_ = 0;
	UINT32 insuranceStartTime_ = 0;
	BOOLEAN contractPriceIncreased_ = FALSE;
	BOOLEAN signedAnotherContract_ = FALSE;
};

// Canonical strategic assignment state. The active and previous assignment,
// training/facility context, elapsed time, squad merge intent, and the
// assignment-specific repair, rest, completion, item-move, and mini-event
// values share one lifecycle owner. Strategic position and travel remain
// separate.
class SoldierAssignmentComponent
{
public:
	INT8& current() noexcept { return current_; }
	const INT8& current() const noexcept { return current_; }
	INT8& previous() noexcept { return previous_; }
	const INT8& previous() const noexcept { return previous_; }
	INT8& trainingStat() noexcept { return trainingStat_; }
	const INT8& trainingStat() const noexcept { return trainingStat_; }
	UINT32& lastChangeMinute() noexcept { return lastChangeMinute_; }
	const UINT32& lastChangeMinute() const noexcept { return lastChangeMinute_; }
	UINT8& desiredSquad() noexcept { return desiredSquad_; }
	const UINT8& desiredSquad() const noexcept { return desiredSquad_; }
	UINT8& mergeTraversalAllowance() noexcept { return mergeTraversalAllowance_; }
	const UINT8& mergeTraversalAllowance() const noexcept { return mergeTraversalAllowance_; }
	UINT8& hours() noexcept { return hours_; }
	const UINT8& hours() const noexcept { return hours_; }
	INT8& repairVehicleId() noexcept { return repairVehicleId_; }
	const INT8& repairVehicleId() const noexcept { return repairVehicleId_; }
	INT16& facilityType() noexcept { return facilityType_; }
	const INT16& facilityType() const noexcept { return facilityType_; }
	UINT8& itemMoveSectorId() noexcept { return itemMoveSectorId_; }
	const UINT8& itemMoveSectorId() const noexcept { return itemMoveSectorId_; }
	UINT16& miniEventHoursRemaining() noexcept { return miniEventHoursRemaining_; }
	const UINT16& miniEventHoursRemaining() const noexcept { return miniEventHoursRemaining_; }
	BOOLEAN& fixingSamSiteState() noexcept { return fixingSamSite_; }
	const BOOLEAN& fixingSamSiteState() const noexcept { return fixingSamSite_; }
	BOOLEAN& fixingRobotState() noexcept { return fixingRobot_; }
	const BOOLEAN& fixingRobotState() const noexcept { return fixingRobot_; }
	BOOLEAN& forcedAwakeState() noexcept { return forcedAwake_; }
	const BOOLEAN& forcedAwakeState() const noexcept { return forcedAwake_; }
	BOOLEAN& assignmentCompleteAndIdleState() noexcept { return assignmentCompleteAndIdle_; }
	const BOOLEAN& assignmentCompleteAndIdleState() const noexcept { return assignmentCompleteAndIdle_; }
	BOOLEAN& asleepState() noexcept { return asleep_; }
	const BOOLEAN& asleepState() const noexcept { return asleep_; }
	BOOLEAN& tiredComplaintState() noexcept { return tiredComplaint_; }
	const BOOLEAN& tiredComplaintState() const noexcept { return tiredComplaint_; }

	bool isAssignedTo(INT8 assignment) const noexcept { return current_ == assignment; }
	bool hasAssignmentHours() const noexcept { return hours_ != 0; }
	bool hasMiniEventTime() const noexcept { return miniEventHoursRemaining_ != 0; }
	bool isFixingSamSite() const noexcept { return fixingSamSite_ != FALSE; }
	bool isFixingRobot() const noexcept { return fixingRobot_ != FALSE; }
	bool isForcedAwake() const noexcept { return forcedAwake_ != FALSE; }
	bool assignmentCompleteAndIdle() const noexcept { return assignmentCompleteAndIdle_ != FALSE; }
	bool isAsleep() const noexcept { return asleep_ != FALSE; }
	bool hasComplainedAboutTiredness() const noexcept { return tiredComplaint_ != FALSE; }
	void clearRepairVehicle() noexcept { repairVehicleId_ = -1; }
	void clearFacility() noexcept { facilityType_ = -1; }
	void setFixingSamSite(BOOLEAN active) noexcept { fixingSamSite_ = active; }
	void setFixingRobot(BOOLEAN active) noexcept { fixingRobot_ = active; }
	void clearRepairTargets() noexcept
	{
		fixingSamSite_ = FALSE;
		fixingRobot_ = FALSE;
	}
	void forceAwake() noexcept { forcedAwake_ = TRUE; }
	void releaseForcedAwake() noexcept { forcedAwake_ = FALSE; }
	void setAssignmentCompleteAndIdle(bool complete) noexcept
	{
		assignmentCompleteAndIdle_ = complete ? TRUE : FALSE;
	}
	void fallAsleep() noexcept { asleep_ = TRUE; }
	void wakeUp() noexcept { asleep_ = FALSE; }
	void markTiredComplaint() noexcept { tiredComplaint_ = TRUE; }
	void clearTiredComplaint() noexcept { tiredComplaint_ = FALSE; }
	void reset() noexcept;

private:
	INT8 current_ = 0;
	INT8 previous_ = 0;
	INT8 trainingStat_ = 0;
	UINT32 lastChangeMinute_ = 0;
	UINT8 desiredSquad_ = 0;
	UINT8 mergeTraversalAllowance_ = 0;
	UINT8 hours_ = 0;
	INT8 repairVehicleId_ = 0;
	INT16 facilityType_ = 0;
	UINT8 itemMoveSectorId_ = 0;
	UINT16 miniEventHoursRemaining_ = 0;
	BOOLEAN fixingSamSite_ = FALSE;
	BOOLEAN fixingRobot_ = FALSE;
	BOOLEAN forcedAwake_ = FALSE;
	BOOLEAN assignmentCompleteAndIdle_ = FALSE;
	BOOLEAN asleep_ = FALSE;
	BOOLEAN tiredComplaint_ = FALSE;
};

// Canonical strategic placement and deployment state. Sector coordinates,
// strategic group and vehicle membership, tactical insertion, traversal
// origin, off-world staging, strategic transit, mission-exit participation,
// landing-zone policy, and arrival bookkeeping move together across the
// strategic/tactical boundary while route and group objects remain adapters.
class SoldierDeploymentComponent
{
public:
	INT8& insertionDirection() noexcept { return insertionDirection_; }
	const INT8& insertionDirection() const noexcept { return insertionDirection_; }
	UINT8& groupId() noexcept { return groupId_; }
	const UINT8& groupId() const noexcept { return groupId_; }
	INT32& insertionGrid() noexcept { return insertionGrid_; }
	const INT32& insertionGrid() const noexcept { return insertionGrid_; }
	UINT8& strategicInsertionCode() noexcept { return strategicInsertionCode_; }
	const UINT8& strategicInsertionCode() const noexcept { return strategicInsertionCode_; }
	INT32& strategicInsertionData() noexcept { return strategicInsertionData_; }
	const INT32& strategicInsertionData() const noexcept { return strategicInsertionData_; }
	INT16& sectorX() noexcept { return sectorX_; }
	const INT16& sectorX() const noexcept { return sectorX_; }
	INT16& sectorY() noexcept { return sectorY_; }
	const INT16& sectorY() const noexcept { return sectorY_; }
	INT8& sectorZ() noexcept { return sectorZ_; }
	const INT8& sectorZ() const noexcept { return sectorZ_; }
	INT32& vehicleId() noexcept { return vehicleId_; }
	const INT32& vehicleId() const noexcept { return vehicleId_; }
	INT32& offWorldGrid() noexcept { return offWorldGrid_; }
	const INT32& offWorldGrid() const noexcept { return offWorldGrid_; }
	UINT8& previousSectorId() noexcept { return previousSectorId_; }
	const UINT8& previousSectorId() const noexcept { return previousSectorId_; }
	UINT8& useExitGridForReentryDirection() noexcept { return useExitGridForReentryDirection_; }
	const UINT8& useExitGridForReentryDirection() const noexcept { return useExitGridForReentryDirection_; }
	INT32& preTraversalGrid() noexcept { return preTraversalGrid_; }
	const INT32& preTraversalGrid() const noexcept { return preTraversalGrid_; }
	UINT8& leaveHistoryCode() noexcept { return leaveHistoryCode_; }
	const UINT8& leaveHistoryCode() const noexcept { return leaveHistoryCode_; }
	UINT32& arrivalTime() noexcept { return arrivalTime_; }
	const UINT32& arrivalTime() const noexcept { return arrivalTime_; }
	BOOLEAN& ignoreCollapseGetupCheck() noexcept { return ignoreCollapseGetupCheck_; }
	const BOOLEAN& ignoreCollapseGetupCheck() const noexcept { return ignoreCollapseGetupCheck_; }
	INT32& arrivalGetupCounter() noexcept { return arrivalGetupCounter_; }
	const INT32& arrivalGetupCounter() const noexcept { return arrivalGetupCounter_; }
	BOOLEAN& waitingForArrivalGetup() noexcept { return waitingForArrivalGetup_; }
	const BOOLEAN& waitingForArrivalGetup() const noexcept { return waitingForArrivalGetup_; }
	BOOLEAN& betweenSectors() noexcept { return betweenSectors_; }
	const BOOLEAN& betweenSectors() const noexcept { return betweenSectors_; }
	BOOLEAN& inMissionExitNode() noexcept { return inMissionExitNode_; }
	const BOOLEAN& inMissionExitNode() const noexcept { return inMissionExitNode_; }
	BOOLEAN& useLandingZoneForArrival() noexcept { return useLandingZoneForArrival_; }
	const BOOLEAN& useLandingZoneForArrival() const noexcept { return useLandingZoneForArrival_; }

	bool isInSector(INT16 x, INT16 y, INT8 z) const noexcept
	{
		return sectorX_ == x && sectorY_ == y && sectorZ_ == z;
	}
	bool hasVehicle() const noexcept { return vehicleId_ >= 0; }
	bool arrivalGetupPending() const noexcept { return waitingForArrivalGetup_ != FALSE; }
	bool isBetweenSectors() const noexcept { return betweenSectors_ != FALSE; }
	bool insideMissionExitNode() const noexcept { return inMissionExitNode_ != FALSE; }
	bool usesLandingZoneForArrival() const noexcept
	{
		return useLandingZoneForArrival_ != FALSE;
	}
	void setSector(INT16 x, INT16 y, INT8 z) noexcept
	{
		sectorX_ = x;
		sectorY_ = y;
		sectorZ_ = z;
	}
	void clearVehicle() noexcept { vehicleId_ = -1; }
	void setStrategicInsertion(UINT8 code, INT32 data) noexcept
	{
		strategicInsertionCode_ = code;
		strategicInsertionData_ = data;
	}
	void setTraversalOrigin(UINT8 previousSectorId, INT32 gridNo) noexcept
	{
		previousSectorId_ = previousSectorId;
		preTraversalGrid_ = gridNo;
	}
	void scheduleArrival(UINT32 time, UINT8 historyCode) noexcept
	{
		arrivalTime_ = time;
		leaveHistoryCode_ = historyCode;
	}
	void beginArrivalGetup() noexcept
	{
		waitingForArrivalGetup_ = TRUE;
		ignoreCollapseGetupCheck_ = TRUE;
	}
	void completeArrivalGetup() noexcept { waitingForArrivalGetup_ = FALSE; }
	void clearCollapseGetupOverride() noexcept { ignoreCollapseGetupCheck_ = FALSE; }
	void beginStrategicTransit() noexcept { betweenSectors_ = TRUE; }
	void completeStrategicTransit() noexcept { betweenSectors_ = FALSE; }
	void enterMissionExitNode() noexcept { inMissionExitNode_ = TRUE; }
	void leaveMissionExitNode() noexcept { inMissionExitNode_ = FALSE; }
	void setUseLandingZoneForArrival(bool enabled) noexcept
	{
		useLandingZoneForArrival_ = enabled ? TRUE : FALSE;
	}
	void reset() noexcept;

private:
	INT8 insertionDirection_ = 0;
	UINT8 groupId_ = 0;
	INT32 insertionGrid_ = 0;
	UINT8 strategicInsertionCode_ = 0;
	INT32 strategicInsertionData_ = 0;
	INT16 sectorX_ = 0;
	INT16 sectorY_ = 0;
	INT8 sectorZ_ = 0;
	INT32 vehicleId_ = -1;
	INT32 offWorldGrid_ = 0;
	UINT8 previousSectorId_ = 0;
	UINT8 useExitGridForReentryDirection_ = 0;
	INT32 preTraversalGrid_ = 0;
	UINT8 leaveHistoryCode_ = 0;
	UINT32 arrivalTime_ = 0;
	BOOLEAN ignoreCollapseGetupCheck_ = FALSE;
	INT32 arrivalGetupCounter_ = 0;
	BOOLEAN waitingForArrivalGetup_ = FALSE;
	BOOLEAN betweenSectors_ = FALSE;
	BOOLEAN inMissionExitNode_ = FALSE;
	BOOLEAN useLandingZoneForArrival_ = FALSE;
};

// Canonical NPC schedule execution state. The schedule identifier and progress
// are shared by the editor, strategic scheduler, and tactical AI. Door
// continuation is kept here as part of that movement lifecycle so its phase
// cannot silently drift away from the grid being operated.
class SoldierScheduleComponent
{
public:
	UINT8& id() noexcept { return id_; }
	const UINT8& id() const noexcept { return id_; }
	INT8& progress() noexcept { return progress_; }
	const INT8& progress() const noexcept { return progress_; }
	INT8& doorOpenPhase() noexcept { return doorOpenPhase_; }
	const INT8& doorOpenPhase() const noexcept { return doorOpenPhase_; }
	INT32& doorGrid() noexcept { return doorGrid_; }
	const INT32& doorGrid() const noexcept { return doorGrid_; }

	bool assigned() const noexcept { return id_ != 0; }
	bool doorContinuationPending() const noexcept { return doorOpenPhase_ != 0; }
	bool doorAnimationStarted() const noexcept { return doorOpenPhase_ == 1; }
	bool doorAnimationComplete() const noexcept { return doorOpenPhase_ == 2; }
	void resetProgress() noexcept { progress_ = 0; }
	void advanceProgress() noexcept;
	void beginDoorContinuation(INT32 gridNo) noexcept;
	void completeDoorAnimation() noexcept;
	INT32 consumeDoorGrid() noexcept;
	void cancelDoorContinuation() noexcept { doorOpenPhase_ = 0; }
	void reset() noexcept;

private:
	UINT8 id_ = 0;
	INT8 progress_ = 0;
	INT8 doorOpenPhase_ = 0;
	INT32 doorGrid_ = 0;
};

// Canonical current tactical world-placement storage. Persistent adapters
// serialize these values at their established, scattered schema positions; the
// component itself is independent of the legacy SOLDIERTYPE declaration.
class SoldierPositionComponent
{
public:
	FLOAT& worldX() noexcept { return worldX_; }
	const FLOAT& worldX() const noexcept { return worldX_; }
	FLOAT& worldY() noexcept { return worldY_; }
	const FLOAT& worldY() const noexcept { return worldY_; }
	INT16& worldXInt() noexcept { return worldXInt_; }
	const INT16& worldXInt() const noexcept { return worldXInt_; }
	INT16& worldYInt() noexcept { return worldYInt_; }
	const INT16& worldYInt() const noexcept { return worldYInt_; }
	INT16& turnStartX() noexcept { return turnStartX_; }
	const INT16& turnStartX() const noexcept { return turnStartX_; }
	INT16& turnStartY() noexcept { return turnStartY_; }
	const INT16& turnStartY() const noexcept { return turnStartY_; }
	INT32& initialGrid() noexcept { return initialGrid_; }
	const INT32& initialGrid() const noexcept { return initialGrid_; }
	INT32& gridNo() noexcept { return gridNo_; }
	const INT32& gridNo() const noexcept { return gridNo_; }
	INT8& level() noexcept { return level_; }
	const INT8& level() const noexcept { return level_; }
	UINT8& direction() noexcept { return direction_; }
	const UINT8& direction() const noexcept { return direction_; }
	INT16& heightAdjustment() noexcept { return heightAdjustment_; }
	const INT16& heightAdjustment() const noexcept { return heightAdjustment_; }
	FLOAT& animationHeightAdjustment() noexcept { return animationHeightAdjustment_; }
	const FLOAT& animationHeightAdjustment() const noexcept { return animationHeightAdjustment_; }
	INT16& desiredHeight() noexcept { return desiredHeight_; }
	const INT16& desiredHeight() const noexcept { return desiredHeight_; }
	INT32& temporaryGrid() noexcept { return temporaryGrid_; }
	const INT32& temporaryGrid() const noexcept { return temporaryGrid_; }
	INT16& roomNo() noexcept { return roomNo_; }
	const INT16& roomNo() const noexcept { return roomNo_; }
	INT8& terrainType() noexcept { return terrainType_; }
	const INT8& terrainType() const noexcept { return terrainType_; }
	INT8& previousTerrainType() noexcept { return previousTerrainType_; }
	const INT8& previousTerrainType() const noexcept { return previousTerrainType_; }

	void setWorldCoordinates(FLOAT x, FLOAT y) noexcept;
	void recordTurnStart(INT16 x, INT16 y) noexcept;
	bool hasTurnStart() const noexcept { return turnStartX_ != 0 || turnStartY_ != 0; }
	void enterTerrain(INT8 terrainType) noexcept;
	void reset() noexcept;

private:
	FLOAT worldX_ = 0;
	FLOAT worldY_ = 0;
	INT16 worldXInt_ = 0;
	INT16 worldYInt_ = 0;
	INT16 turnStartX_ = 0;
	INT16 turnStartY_ = 0;
	INT32 initialGrid_ = 0;
	INT32 gridNo_ = 0;
	INT8 level_ = 0;
	UINT8 direction_ = 0;
	INT16 heightAdjustment_ = 0;
	FLOAT animationHeightAdjustment_ = 0;
	INT16 desiredHeight_ = 0;
	INT32 temporaryGrid_ = 0;
	INT16 roomNo_ = 0;
	INT8 terrainType_ = 0;
	INT8 previousTerrainType_ = 0;
};

// Canonical history of tactical grid movement. Current placement belongs to
// SoldierPositionComponent; this component owns the grid departed most
// recently and the bounded two-location history used to stop AI oscillation.
class SoldierMovementHistoryComponent
{
public:
	using RecentLocations = INT32[2];

	INT32& previousGrid() noexcept { return previousGrid_; }
	const INT32& previousGrid() const noexcept { return previousGrid_; }
	RecentLocations& recentLocations() noexcept { return recentLocations_; }
	const RecentLocations& recentLocations() const noexcept { return recentLocations_; }

	void recordDeparture(INT32 gridNo) noexcept { previousGrid_ = gridNo; }
	void resetAiLoop() noexcept;
	bool observeAiMovement(
		INT32 currentGrid, INT32 destinationGrid, INT32 gridCount) noexcept;
	void reset() noexcept;

private:
	static constexpr INT32 NoGrid = -1;

	INT32 previousGrid_ = 0;
	RecentLocations recentLocations_{};
};

// Canonical tactical route ownership. The fixed-capacity path and its cursor
// deliberately retain the established JA2 representation, while private
// storage prevents unrelated SOLDIERTYPE fields from becoming a second route
// authority.
class SoldierPathingComponent
{
public:
	using Path = UINT16[MAX_PATH_LIST_SIZE];

	INT8& desiredDirection() noexcept { return desiredDirection_; }
	const INT8& desiredDirection() const noexcept { return desiredDirection_; }
	INT16& destinationX() noexcept { return destinationX_; }
	const INT16& destinationX() const noexcept { return destinationX_; }
	INT16& destinationY() noexcept { return destinationY_; }
	const INT16& destinationY() const noexcept { return destinationY_; }
	INT32& destinationGrid() noexcept { return destinationGrid_; }
	const INT32& destinationGrid() const noexcept { return destinationGrid_; }
	INT32& finalDestinationGrid() noexcept { return finalDestinationGrid_; }
	const INT32& finalDestinationGrid() const noexcept { return finalDestinationGrid_; }
	INT8& stopped() noexcept { return stopped_; }
	const INT8& stopped() const noexcept { return stopped_; }
	INT8& needsLook() noexcept { return needsLook_; }
	const INT8& needsLook() const noexcept { return needsLook_; }
	Path& path() noexcept { return path_; }
	const Path& path() const noexcept { return path_; }
	UINT16& pathSize() noexcept { return pathSize_; }
	const UINT16& pathSize() const noexcept { return pathSize_; }
	UINT16& pathIndex() noexcept { return pathIndex_; }
	const UINT16& pathIndex() const noexcept { return pathIndex_; }
	INT32& blackListGrid() noexcept { return blackListGrid_; }
	const INT32& blackListGrid() const noexcept { return blackListGrid_; }
	INT8& stored() noexcept { return stored_; }
	const INT8& stored() const noexcept { return stored_; }

	bool empty() const noexcept { return pathSize_ == 0; }
	bool complete() const noexcept { return pathIndex_ >= pathSize_; }
	void clearRoute() noexcept;
	void reset() noexcept;

private:
	INT8 desiredDirection_ = 0;
	INT16 destinationX_ = 0;
	INT16 destinationY_ = 0;
	INT32 destinationGrid_ = 0;
	INT32 finalDestinationGrid_ = 0;
	INT8 stopped_ = 0;
	INT8 needsLook_ = 0;
	Path path_{};
	UINT16 pathSize_ = 0;
	UINT16 pathIndex_ = 0;
	INT32 blackListGrid_ = 0;
	INT8 stored_ = 0;
};

// Canonical tactical movement intent and contention state. Route geometry
// belongs to SoldierPathingComponent; this component owns the selected
// movement-animation mode and mutable state used while executing that route
// around reservations and other soldiers. Turn ownership, UI speed, AP
// exhaustion, pause/water edges, movement timing, and destination-center
// crossing share the same lifecycle boundary.
class SoldierMovementComponent
{
public:
	INT16& mode() noexcept { return mode_; }
	const INT16& mode() const noexcept { return mode_; }
	INT8& stealthMode() noexcept { return stealthMode_; }
	const INT8& stealthMode() const noexcept { return stealthMode_; }
	INT8& reverse() noexcept { return reverse_; }
	const INT8& reverse() const noexcept { return reverse_; }
	UINT8& highResolutionDirection() noexcept { return highResolutionDirection_; }
	const UINT8& highResolutionDirection() const noexcept { return highResolutionDirection_; }
	UINT8& highResolutionDesiredDirection() noexcept { return highResolutionDesiredDirection_; }
	const UINT8& highResolutionDesiredDirection() const noexcept { return highResolutionDesiredDirection_; }
	INT8& animationDirection() noexcept { return animationDirection_; }
	const INT8& animationDirection() const noexcept { return animationDirection_; }
	UINT16& gridUpdatePolicy() noexcept { return gridUpdatePolicy_; }
	const UINT16& gridUpdatePolicy() const noexcept { return gridUpdatePolicy_; }
	UINT8& delayCounter() noexcept { return delayCounter_; }
	const UINT8& delayCounter() const noexcept { return delayCounter_; }
	INT32& delayedCauseGrid() noexcept { return delayedCauseGrid_; }
	const INT32& delayedCauseGrid() const noexcept { return delayedCauseGrid_; }
	INT32& reservedGrid() noexcept { return reservedGrid_; }
	const INT32& reservedGrid() const noexcept { return reservedGrid_; }
	BOOLEAN& blockedByAnotherMerc() noexcept { return blockedByAnotherMerc_; }
	const BOOLEAN& blockedByAnotherMerc() const noexcept { return blockedByAnotherMerc_; }
	INT8& blockedDirection() noexcept { return blockedDirection_; }
	const INT8& blockedDirection() const noexcept { return blockedDirection_; }
	INT32& absoluteDestination() noexcept { return absoluteDestination_; }
	const INT32& absoluteDestination() const noexcept { return absoluteDestination_; }
	INT32& continuedPathGrid() noexcept { return continuedPathGrid_; }
	const INT32& continuedPathGrid() const noexcept { return continuedPathGrid_; }
	INT8& continuedPathValid() noexcept { return continuedPathValid_; }
	const INT8& continuedPathValid() const noexcept { return continuedPathValid_; }
	UINT8& delayedFlags() noexcept { return delayedFlags_; }
	const UINT8& delayedFlags() const noexcept { return delayedFlags_; }
	UINT8& stopReason() noexcept { return stopReason_; }
	const UINT8& stopReason() const noexcept { return stopReason_; }
	SoldierID& moveSpeedOverride() noexcept { return moveSpeedOverride_; }
	const SoldierID& moveSpeedOverride() const noexcept { return moveSpeedOverride_; }
	BOOLEAN& usesMoveSpeedOverride() noexcept { return usesMoveSpeedOverride_; }
	const BOOLEAN& usesMoveSpeedOverride() const noexcept { return usesMoveSpeedOverride_; }
	BOOLEAN& turnInProgress() noexcept { return turnInProgress_; }
	const BOOLEAN& turnInProgress() const noexcept { return turnInProgress_; }
	BOOLEAN& previousInWater() noexcept { return previousInWater_; }
	const BOOLEAN& previousInWater() const noexcept { return previousInWater_; }
	BOOLEAN& uiMovementFast() noexcept { return uiMovementFast_; }
	const BOOLEAN& uiMovementFast() const noexcept { return uiMovementFast_; }
	BOOLEAN& noActionPointsToFinish() noexcept { return noActionPointsToFinish_; }
	const BOOLEAN& noActionPointsToFinish() const noexcept { return noActionPointsToFinish_; }
	BOOLEAN& paused() noexcept { return paused_; }
	const BOOLEAN& paused() const noexcept { return paused_; }
	BOOLEAN& movementClockActive() noexcept { return movementClockActive_; }
	const BOOLEAN& movementClockActive() const noexcept { return movementClockActive_; }
	BOOLEAN& networkDelayed() noexcept { return networkDelayed_; }
	const BOOLEAN& networkDelayed() const noexcept { return networkDelayed_; }
	BOOLEAN& wasMoving() noexcept { return wasMoving_; }
	const BOOLEAN& wasMoving() const noexcept { return wasMoving_; }
	INT8& pastXDestination() noexcept { return pastXDestination_; }
	const INT8& pastXDestination() const noexcept { return pastXDestination_; }
	INT8& pastYDestination() noexcept { return pastYDestination_; }
	const INT8& pastYDestination() const noexcept { return pastYDestination_; }
	UINT8& waitAction() noexcept { return waitAction_; }
	const UINT8& waitAction() const noexcept { return waitAction_; }

	bool stealthy() const noexcept { return stealthMode_ != FALSE; }
	bool reversing() const noexcept { return reverse_ != FALSE; }
	bool delayed() const noexcept { return delayCounter_ != 0; }
	bool turnActive() const noexcept { return turnInProgress_ != FALSE; }
	bool wasInWater() const noexcept { return previousInWater_ != FALSE; }
	bool fastUiMovement() const noexcept { return uiMovementFast_ != FALSE; }
	bool outOfActionPoints() const noexcept { return noActionPointsToFinish_ != FALSE; }
	bool movementPaused() const noexcept { return paused_ != FALSE; }
	bool recordingMovement() const noexcept { return movementClockActive_ != FALSE; }
	bool delayedByNetwork() const noexcept { return networkDelayed_ != FALSE; }
	bool waitingForAction() const noexcept { return waitAction_ != 0; }
	bool crossedDestinationCenter() const noexcept
	{
		return pastXDestination_ != FALSE && pastYDestination_ != FALSE;
	}
	void setStealth(bool enabled) noexcept { stealthMode_ = enabled ? TRUE : FALSE; }
	void setReverse(bool enabled) noexcept { reverse_ = enabled ? TRUE : FALSE; }
	void setHighResolutionFacing(UINT8 current, UINT8 desired) noexcept
	{
		highResolutionDirection_ = current;
		highResolutionDesiredDirection_ = desired;
	}
	void requestGridUpdateSuppression() noexcept { gridUpdatePolicy_ = 1; }
	void clearGridUpdatePolicy() noexcept { gridUpdatePolicy_ = 0; }
	void waitForGrid(INT32 gridNo, UINT8 counter) noexcept;
	void clearDelay() noexcept { delayCounter_ = 0; }
	void blockInDirection(INT8 direction) noexcept;
	void clearBlock() noexcept { blockedByAnotherMerc_ = FALSE; }
	void setContinuedPath(INT32 gridNo) noexcept;
	void clearContinuedPath() noexcept { continuedPathValid_ = FALSE; }
	void overrideMoveSpeedWith(SoldierID soldier) noexcept;
	void clearMoveSpeedOverride() noexcept { usesMoveSpeedOverride_ = FALSE; }
	void beginTurn() noexcept { turnInProgress_ = TRUE; }
	void finishTurn() noexcept { turnInProgress_ = FALSE; }
	void rememberWaterState(bool inWater) noexcept { previousInWater_ = inWater ? TRUE : FALSE; }
	void setUiMovementFast(BOOLEAN mode) noexcept { uiMovementFast_ = mode; }
	void clearUiMovementFast() noexcept { uiMovementFast_ = FALSE; }
	void setOutOfActionPoints(bool exhausted) noexcept
	{
		noActionPointsToFinish_ = exhausted ? TRUE : FALSE;
	}
	void pauseMovement() noexcept { paused_ = TRUE; }
	void resumeMovement() noexcept { paused_ = FALSE; }
	void startMovementClock() noexcept { movementClockActive_ = TRUE; }
	void stopMovementClock() noexcept { movementClockActive_ = FALSE; }
	void setNetworkDelayed(bool delayed) noexcept { networkDelayed_ = delayed ? TRUE : FALSE; }
	bool syncPresentationMotion(bool moving) noexcept
	{
		const BOOLEAN next = moving ? TRUE : FALSE;
		if (wasMoving_ == next)
		{
			return false;
		}
		wasMoving_ = next;
		return true;
	}
	void markPastXDestination() noexcept { pastXDestination_ = TRUE; }
	void markPastYDestination() noexcept { pastYDestination_ = TRUE; }
	void clearPastDestination() noexcept
	{
		pastXDestination_ = FALSE;
		pastYDestination_ = FALSE;
	}
	void requestWaitAction(UINT8 action) noexcept { waitAction_ = action; }
	void clearWaitAction() noexcept { waitAction_ = 0; }
	void reset() noexcept;

private:
	INT16 mode_ = 0;
	INT8 stealthMode_ = 0;
	INT8 reverse_ = 0;
	UINT8 highResolutionDirection_ = 0;
	UINT8 highResolutionDesiredDirection_ = 0;
	INT8 animationDirection_ = 0;
	UINT16 gridUpdatePolicy_ = 0;
	UINT8 delayCounter_ = 0;
	INT32 delayedCauseGrid_ = 0;
	INT32 reservedGrid_ = 0;
	BOOLEAN blockedByAnotherMerc_ = FALSE;
	INT8 blockedDirection_ = 0;
	INT32 absoluteDestination_ = 0;
	INT32 continuedPathGrid_ = 0;
	INT8 continuedPathValid_ = FALSE;
	UINT8 delayedFlags_ = 0;
	UINT8 stopReason_ = 0;
	SoldierID moveSpeedOverride_{};
	BOOLEAN usesMoveSpeedOverride_ = FALSE;
	BOOLEAN turnInProgress_ = FALSE;
	BOOLEAN previousInWater_ = FALSE;
	BOOLEAN uiMovementFast_ = FALSE;
	BOOLEAN noActionPointsToFinish_ = FALSE;
	BOOLEAN paused_ = FALSE;
	BOOLEAN movementClockActive_ = FALSE;
	BOOLEAN networkDelayed_ = FALSE;
	BOOLEAN wasMoving_ = FALSE;
	INT8 pastXDestination_ = FALSE;
	INT8 pastYDestination_ = FALSE;
	UINT8 waitAction_ = 0;
};

// Canonical tactical turn and interrupt state. The scheduler's moved state,
// interrupt duel budget/snapshot, and per-opponent interrupt counters share
// one owner so interrupt entry and restoration cannot diverge.
class SoldierTurnStateComponent
{
public:
	using InterruptCounters = UINT8[MAX_NUM_SOLDIERS];

	INT8& interruptDuelPoints() noexcept { return interruptDuelPoints_; }
	const INT8& interruptDuelPoints() const noexcept { return interruptDuelPoints_; }
	INT8& passedLastInterrupt() noexcept { return passedLastInterrupt_; }
	const INT8& passedLastInterrupt() const noexcept { return passedLastInterrupt_; }
	INT16& interruptStartActionPoints() noexcept { return interruptStartActionPoints_; }
	const INT16& interruptStartActionPoints() const noexcept { return interruptStartActionPoints_; }
	INT8& moved() noexcept { return moved_; }
	const INT8& moved() const noexcept { return moved_; }
	INT8& movedBeforeInterrupt() noexcept { return movedBeforeInterrupt_; }
	const INT8& movedBeforeInterrupt() const noexcept { return movedBeforeInterrupt_; }
	InterruptCounters& interruptCounters() noexcept { return interruptCounters_; }
	const InterruptCounters& interruptCounters() const noexcept { return interruptCounters_; }

	void captureMoved(INT8 moved) noexcept { movedBeforeInterrupt_ = moved; }
	void reset() noexcept;

private:
	INT8 interruptDuelPoints_ = 0;
	INT8 passedLastInterrupt_ = 0;
	INT16 interruptStartActionPoints_ = 0;
	INT8 moved_ = 0;
	INT8 movedBeforeInterrupt_ = 0;
	InterruptCounters interruptCounters_{};
};

// Canonical tactical target selection. Attack execution, UI, AI, and network
// adapters all observe this same target geometry and identity; keeping it
// private prevents the legacy SOLDIERTYPE field list from becoming a second
// mutable authority.
class SoldierTargetingComponent
{
public:
	INT32& gridNo() noexcept { return gridNo_; }
	const INT32& gridNo() const noexcept { return gridNo_; }
	INT8& level() noexcept { return level_; }
	const INT8& level() const noexcept { return level_; }
	INT8& cubeLevel() noexcept { return cubeLevel_; }
	const INT8& cubeLevel() const noexcept { return cubeLevel_; }
	INT32& lastGridNo() noexcept { return lastGridNo_; }
	const INT32& lastGridNo() const noexcept { return lastGridNo_; }
	SoldierID& targetId() noexcept { return targetId_; }
	const SoldierID& targetId() const noexcept { return targetId_; }
	SoldierID& engagedOpponent() noexcept { return engagedOpponent_; }
	const SoldierID& engagedOpponent() const noexcept { return engagedOpponent_; }
	SoldierID& lineOfFireTarget() noexcept { return lineOfFireTarget_; }
	const SoldierID& lineOfFireTarget() const noexcept { return lineOfFireTarget_; }
	BOOLEAN& intendedTarget() noexcept { return intendedTarget_; }
	const BOOLEAN& intendedTarget() const noexcept { return intendedTarget_; }
	BOOLEAN& retainLastTargetFromTurn() noexcept
	{
		return retainLastTargetFromTurn_;
	}
	const BOOLEAN& retainLastTargetFromTurn() const noexcept
	{
		return retainLastTargetFromTurn_;
	}

	bool hasTargetSoldier() const noexcept { return targetId_ != NOBODY; }
	bool hasEngagedOpponent() const noexcept { return engagedOpponent_ != NOBODY; }
	bool hasLineOfFireTarget() const noexcept { return lineOfFireTarget_ != NOBODY; }
	void selectLocation(INT32 gridNo, INT8 level, INT8 cubeLevel = 0) noexcept;
	void selectSoldier(SoldierID target) noexcept { targetId_ = target; }
	void clearTargetSoldier() noexcept { targetId_ = NOBODY; }
	void engageOpponent(SoldierID target) noexcept { engagedOpponent_ = target; }
	void clearEngagedOpponent() noexcept { engagedOpponent_ = NOBODY; }
	void rememberLineOfFireTarget(SoldierID target) noexcept { lineOfFireTarget_ = target; }
	void clearLineOfFireTarget() noexcept { lineOfFireTarget_ = NOBODY; }
	void reset() noexcept;

private:
	INT32 gridNo_ = 0;
	INT8 level_ = 0;
	INT8 cubeLevel_ = 0;
	INT32 lastGridNo_ = 0;
	SoldierID targetId_ = NOBODY;
	SoldierID engagedOpponent_ = NOBODY;
	SoldierID lineOfFireTarget_ = NOBODY;
	BOOLEAN intendedTarget_ = FALSE;
	BOOLEAN retainLastTargetFromTurn_ = FALSE;
};

// Canonical weapon and aim selection for one tactical actor. Target geometry
// belongs to SoldierTargetingComponent; this component owns how that target is
// attacked so UI, AI, replay, and network adapters cannot diverge through
// independent flat SOLDIERTYPE fields.
class SoldierAttackSelectionComponent
{
public:
	UINT8& hand() noexcept { return hand_; }
	const UINT8& hand() const noexcept { return hand_; }
	UINT16& weapon() noexcept { return weapon_; }
	const UINT16& weapon() const noexcept { return weapon_; }
	INT8& weaponMode() noexcept { return weaponMode_; }
	const INT8& weaponMode() const noexcept { return weaponMode_; }
	INT8& scopeMode() noexcept { return scopeMode_; }
	const INT8& scopeMode() const noexcept { return scopeMode_; }
	UINT8& shotLocation() noexcept { return shotLocation_; }
	const UINT8& shotLocation() const noexcept { return shotLocation_; }
	UINT8& meleeLocation() noexcept { return meleeLocation_; }
	const UINT8& meleeLocation() const noexcept { return meleeLocation_; }

	void selectWeapon(UINT8 hand, UINT16 weapon) noexcept;
	void reset() noexcept;

private:
	UINT8 hand_ = 0;
	UINT16 weapon_ = 0;
	INT8 weaponMode_ = 0;
	INT8 scopeMode_ = 0;
	UINT8 shotLocation_ = 0;
	UINT8 meleeLocation_ = 0;
};

// Canonical cache for the path portion of a melee attack. The target grid and
// movement mode identify a reusable calculation; cost and terminal direction
// are the corresponding result. Movement invalidates the target without
// changing the established serialized defaults.
class SoldierMeleeApproachComponent
{
public:
	INT16& movementMode() noexcept { return movementMode_; }
	const INT16& movementMode() const noexcept { return movementMode_; }
	INT32& grid() noexcept { return grid_; }
	const INT32& grid() const noexcept { return grid_; }
	INT16& cost() noexcept { return cost_; }
	const INT16& cost() const noexcept { return cost_; }
	UINT8& endDirection() noexcept { return endDirection_; }
	const UINT8& endDirection() const noexcept { return endDirection_; }

	bool matches(INT32 grid, INT16 movementMode) const noexcept
	{
		return grid_ == grid && movementMode_ == movementMode;
	}
	void recordPath(INT16 movementMode, INT16 cost, UINT8 endDirection) noexcept
	{
		movementMode_ = movementMode;
		cost_ = cost;
		endDirection_ = endDirection;
	}
	void rememberGrid(INT32 grid) noexcept { grid_ = grid; }
	void clearCost() noexcept { cost_ = 0; }
	void invalidate() noexcept { grid_ = NoGrid; }
	void reset() noexcept;

private:
	static constexpr INT32 NoGrid = -1;

	INT16 movementMode_ = 0;
	INT32 grid_ = 0;
	INT16 cost_ = 0;
	UINT8 endDirection_ = 0;
};

// Canonical state for selecting and executing one firing volley. Weapon and
// aim choice belong to SoldierAttackSelectionComponent; this component owns
// burst/autofire progress, spread targets, recoil history, and multi-barrel
// bookkeeping that must evolve together once firing begins.
class SoldierFireControlComponent
{
public:
	static constexpr UINT8 SpreadTargetCapacity = 6;
	using OffsetHistory = FLOAT[2];
	using SpreadLocations = INT32[SpreadTargetCapacity];

	INT8& burstCounter() noexcept { return burstCounter_; }
	const INT8& burstCounter() const noexcept { return burstCounter_; }
	UINT8& autofireShots() noexcept { return autofireShots_; }
	const UINT8& autofireShots() const noexcept { return autofireShots_; }
	INT8& bulletsLeft() noexcept { return bulletsLeft_; }
	const INT8& bulletsLeft() const noexcept { return bulletsLeft_; }
	BOOLEAN& spreadIndex() noexcept { return spreadIndex_; }
	const BOOLEAN& spreadIndex() const noexcept { return spreadIndex_; }
	BOOLEAN& autofireLastStep() noexcept { return autofireLastStep_; }
	const BOOLEAN& autofireLastStep() const noexcept { return autofireLastStep_; }
	SpreadLocations& spreadLocations() noexcept { return spreadLocations_; }
	const SpreadLocations& spreadLocations() const noexcept { return spreadLocations_; }
	OffsetHistory& previousMuzzleOffsetX() noexcept { return previousMuzzleOffsetX_; }
	const OffsetHistory& previousMuzzleOffsetX() const noexcept { return previousMuzzleOffsetX_; }
	OffsetHistory& previousMuzzleOffsetY() noexcept { return previousMuzzleOffsetY_; }
	const OffsetHistory& previousMuzzleOffsetY() const noexcept { return previousMuzzleOffsetY_; }
	OffsetHistory& previousCounterForceX() noexcept { return previousCounterForceX_; }
	const OffsetHistory& previousCounterForceX() const noexcept { return previousCounterForceX_; }
	OffsetHistory& previousCounterForceY() noexcept { return previousCounterForceY_; }
	const OffsetHistory& previousCounterForceY() const noexcept { return previousCounterForceY_; }
	FLOAT& initialMuzzleOffsetX() noexcept { return initialMuzzleOffsetX_; }
	const FLOAT& initialMuzzleOffsetX() const noexcept { return initialMuzzleOffsetX_; }
	FLOAT& initialMuzzleOffsetY() noexcept { return initialMuzzleOffsetY_; }
	const FLOAT& initialMuzzleOffsetY() const noexcept { return initialMuzzleOffsetY_; }
	UINT8& barrelCounter() noexcept { return barrelCounter_; }
	const UINT8& barrelCounter() const noexcept { return barrelCounter_; }
	INT32& spreadDragStartGrid() noexcept { return spreadDragStartGrid_; }
	const INT32& spreadDragStartGrid() const noexcept { return spreadDragStartGrid_; }
	INT32& spreadDragEndGrid() noexcept { return spreadDragEndGrid_; }
	const INT32& spreadDragEndGrid() const noexcept { return spreadDragEndGrid_; }
	INT8& gunType() noexcept { return gunType_; }
	const INT8& gunType() const noexcept { return gunType_; }
	UINT8& grenadeLauncherDelayMode() noexcept { return grenadeLauncherDelayMode_; }
	const UINT8& grenadeLauncherDelayMode() const noexcept { return grenadeLauncherDelayMode_; }
	UINT8& barrelMode() noexcept { return barrelMode_; }
	const UINT8& barrelMode() const noexcept { return barrelMode_; }
	BOOLEAN& reloading() noexcept { return reloading_; }
	const BOOLEAN& reloading() const noexcept { return reloading_; }
	BOOLEAN& aimPaused() noexcept { return aimPaused_; }
	const BOOLEAN& aimPaused() const noexcept { return aimPaused_; }

	bool bursting() const noexcept { return burstCounter_ != 0; }
	bool autofiring() const noexcept { return autofireShots_ != 0; }
	bool delaysGrenadeLauncherExplosion() const noexcept
	{
		return grenadeLauncherDelayMode_ != 0;
	}
	bool spreadDragMoved() const noexcept
	{
		return spreadDragEndGrid_ != spreadDragStartGrid_;
	}
	static constexpr UINT8 clampSpreadTargetCount(UINT16 requested) noexcept
	{
		return requested > SpreadTargetCapacity
			? SpreadTargetCapacity
			: static_cast<UINT8>(requested);
	}
	void selectSingleShot() noexcept;
	void selectBurst() noexcept;
	void selectAutofire(UINT8 shots = 1) noexcept;
	void clearSpreadTargets() noexcept;
	void beginSpreadDrag(INT32 grid) noexcept { spreadDragStartGrid_ = grid; }
	void updateSpreadDrag(INT32 grid) noexcept { spreadDragEndGrid_ = grid; }
	void setGrenadeLauncherDelay(bool enabled) noexcept
	{
		grenadeLauncherDelayMode_ = enabled ? 1 : 0;
	}
	void selectBarrelMode(UINT8 mode) noexcept { barrelMode_ = mode; }
	void reset() noexcept;

private:
	INT8 burstCounter_ = 0;
	UINT8 autofireShots_ = 0;
	INT8 bulletsLeft_ = 0;
	BOOLEAN spreadIndex_ = FALSE;
	BOOLEAN autofireLastStep_ = FALSE;
	SpreadLocations spreadLocations_{};
	OffsetHistory previousMuzzleOffsetX_{};
	OffsetHistory previousMuzzleOffsetY_{};
	OffsetHistory previousCounterForceX_{};
	OffsetHistory previousCounterForceY_{};
	FLOAT initialMuzzleOffsetX_ = 0.0f;
	FLOAT initialMuzzleOffsetY_ = 0.0f;
	UINT8 barrelCounter_ = 0;
	INT32 spreadDragStartGrid_ = 0;
	INT32 spreadDragEndGrid_ = 0;
	INT8 gunType_ = 0;
	UINT8 grenadeLauncherDelayMode_ = 0;
	UINT8 barrelMode_ = 0;
	BOOLEAN reloading_ = FALSE;
	BOOLEAN aimPaused_ = FALSE;
};

// Canonical result of incoming combat. This keeps the current/previous
// attacker chain and hit metadata together without mixing it with outgoing
// target selection or damage-number presentation.
class SoldierCombatResultComponent
{
public:
	SoldierID& currentAttacker() noexcept { return currentAttacker_; }
	const SoldierID& currentAttacker() const noexcept { return currentAttacker_; }
	SoldierID& previousAttacker() noexcept { return previousAttacker_; }
	const SoldierID& previousAttacker() const noexcept { return previousAttacker_; }
	SoldierID& earlierAttacker() noexcept { return earlierAttacker_; }
	const SoldierID& earlierAttacker() const noexcept { return earlierAttacker_; }
	UINT8& hitLocation() noexcept { return hitLocation_; }
	const UINT8& hitLocation() const noexcept { return hitLocation_; }
	UINT8& lastDamageReason() noexcept { return lastDamageReason_; }
	const UINT8& lastDamageReason() const noexcept { return lastDamageReason_; }
	INT8& hitsThisTurn() noexcept { return hitsThisTurn_; }
	const INT8& hitsThisTurn() const noexcept { return hitsThisTurn_; }
	INT8& pelletsHitBy() noexcept { return pelletsHitBy_; }
	const INT8& pelletsHitBy() const noexcept { return pelletsHitBy_; }
	INT16& accumulatedDamage() noexcept { return accumulatedDamage_; }
	const INT16& accumulatedDamage() const noexcept { return accumulatedDamage_; }
	INT8& lastAttackHit() noexcept { return lastAttackHit_; }
	const INT8& lastAttackHit() const noexcept { return lastAttackHit_; }

	bool hasCurrentAttacker() const noexcept { return currentAttacker_ != NOBODY; }
	void recordHit(SoldierID attacker, UINT8 location) noexcept;
	void advanceAttackerHistory(bool retainCurrent) noexcept;
	void restorePreviousAttacker() noexcept;
	void clearAttackers() noexcept;
	void reset() noexcept;

private:
	SoldierID currentAttacker_ = NOBODY;
	SoldierID previousAttacker_ = NOBODY;
	SoldierID earlierAttacker_ = NOBODY;
	UINT8 hitLocation_ = 0;
	UINT8 lastDamageReason_ = 0;
	INT8 hitsThisTurn_ = 0;
	INT8 pelletsHitBy_ = 0;
	INT16 accumulatedDamage_ = 0;
	INT8 lastAttackHit_ = 0;
};

// Canonical outgoing combat-credit record. Tactical combat and autoresolve
// accrue militia promotion credit here, while the fixed per-team damage table
// retains the established assist-attribution save payload.
class SoldierCombatContributionComponent
{
public:
	using DamageByTeam = UINT8[NUM_ASSIST_SLOTS];

	UINT8& militiaKills() noexcept { return militiaKills_; }
	const UINT8& militiaKills() const noexcept { return militiaKills_; }
	UINT8& militiaAssists() noexcept { return militiaAssists_; }
	const UINT8& militiaAssists() const noexcept { return militiaAssists_; }
	DamageByTeam& damageByTeam() noexcept { return damageByTeam_; }
	const DamageByTeam& damageByTeam() const noexcept { return damageByTeam_; }

	bool hasMilitiaKills() const noexcept { return militiaKills_ != 0; }
	bool hasMilitiaCredit() const noexcept
	{
		return militiaKills_ != 0 || militiaAssists_ != 0;
	}
	UINT16 militiaPromotionPoints() const noexcept
	{
		return static_cast<UINT16>(2 * militiaKills_ + militiaAssists_);
	}
	void recordMilitiaKill() noexcept;
	void recordMilitiaAssist() noexcept;
	void clearMilitiaCredit() noexcept;
	void reset() noexcept;

private:
	UINT8 militiaKills_ = 0;
	UINT8 militiaAssists_ = 0;
	DamageByTeam damageByTeam_{};
};

// Canonical reaction to hostile fire. This state is consumed by both combat
// rules and AI decisions, so it is independent of generic AI scratch and
// presentation-only feedback from the most recent attack.
class SoldierSuppressionComponent
{
public:
	INT8& underFire() noexcept { return underFire_; }
	const INT8& underFire() const noexcept { return underFire_; }
	INT8& shock() noexcept { return shock_; }
	const INT8& shock() const noexcept { return shock_; }
	UINT8& points() noexcept { return points_; }
	const UINT8& points() const noexcept { return points_; }
	UINT8& actionPointsLost() noexcept { return actionPointsLost_; }
	const UINT8& actionPointsLost() const noexcept { return actionPointsLost_; }
	SoldierID& suppressor() noexcept { return suppressor_; }
	const SoldierID& suppressor() const noexcept { return suppressor_; }
	INT8& closeCall() noexcept { return closeCall_; }
	const INT8& closeCall() const noexcept { return closeCall_; }

	bool active() const noexcept { return points_ != 0; }
	bool hasSuppressor() const noexcept { return suppressor_ != NOBODY; }
	void addPoints(UINT16 amount) noexcept;
	void recordBullet(SoldierID suppressor) noexcept;
	void addActionPointLoss(UINT16 amount) noexcept;
	void markCloseCall() noexcept { closeCall_ = TRUE; }
	void clearCloseCall() noexcept { closeCall_ = FALSE; }
	void clearAttackPoints() noexcept { points_ = 0; }
	void beginTurn() noexcept;
	void reset() noexcept;

private:
	INT8 underFire_ = 0;
	INT8 shock_ = 0;
	UINT8 points_ = 0;
	UINT8 actionPointsLost_ = 0;
	SoldierID suppressor_ = NOBODY;
	INT8 closeCall_ = FALSE;
};

// Presentation payload for the floating tactical damage number. Simulation
// damage and attribution stay in SoldierCombatResultComponent; this state only
// tracks the animation cursor, screen offset, and display direction.
class SoldierDamageDisplayComponent
{
public:
	INT8& displayFlag() noexcept { return displayFlag_; }
	const INT8& displayFlag() const noexcept { return displayFlag_; }
	INT8& counter() noexcept { return counter_; }
	const INT8& counter() const noexcept { return counter_; }
	INT16& offsetX() noexcept { return offsetX_; }
	const INT16& offsetX() const noexcept { return offsetX_; }
	INT16& offsetY() noexcept { return offsetY_; }
	const INT16& offsetY() const noexcept { return offsetY_; }
	INT8& direction() noexcept { return direction_; }
	const INT8& direction() const noexcept { return direction_; }

	bool displaying() const noexcept { return displayFlag_ != 0; }
	bool expired() const noexcept { return counter_ >= 8; }
	void restart() noexcept;
	void activateAt(INT16 offsetX, INT16 offsetY) noexcept;
	void advance() noexcept;
	void clear() noexcept;
	void reset() noexcept;

private:
	INT8 displayFlag_ = 0;
	INT8 counter_ = 0;
	INT16 offsetX_ = 0;
	INT16 offsetY_ = 0;
	INT8 direction_ = 0;
};

// Canonical value state consumed by soldier rendering. Palette resource
// pointers remain legacy adapters, while their stable replacement identities,
// fade lifecycle, light handles, redraw bounds, and screen geometry share one
// reset boundary.
class SoldierRenderStateComponent
{
public:
	static constexpr INT32 NoLightSprite = -1;

	PaletteRepID& headPalette() noexcept { return headPalette_; }
	const PaletteRepID& headPalette() const noexcept { return headPalette_; }
	PaletteRepID& pantsPalette() noexcept { return pantsPalette_; }
	const PaletteRepID& pantsPalette() const noexcept { return pantsPalette_; }
	PaletteRepID& vestPalette() noexcept { return vestPalette_; }
	const PaletteRepID& vestPalette() const noexcept { return vestPalette_; }
	PaletteRepID& skinPalette() noexcept { return skinPalette_; }
	const PaletteRepID& skinPalette() const noexcept { return skinPalette_; }
	PaletteRepID& miscPalette() noexcept { return miscPalette_; }
	const PaletteRepID& miscPalette() const noexcept { return miscPalette_; }
	UINT8& fadeMode() noexcept { return fadeMode_; }
	const UINT8& fadeMode() const noexcept { return fadeMode_; }
	BOOLEAN& forceRenderColor() noexcept { return forceRenderColor_; }
	const BOOLEAN& forceRenderColor() const noexcept { return forceRenderColor_; }
	BOOLEAN& forceNoPaletteCycle() noexcept { return forceNoPaletteCycle_; }
	const BOOLEAN& forceNoPaletteCycle() const noexcept { return forceNoPaletteCycle_; }
	BOOLEAN& forceShade() noexcept { return forceShade_; }
	const BOOLEAN& forceShade() const noexcept { return forceShade_; }
	BOOLEAN& muzzleFlashVisible() noexcept { return muzzleFlashVisible_; }
	const BOOLEAN& muzzleFlashVisible() const noexcept { return muzzleFlashVisible_; }
	UINT8& fadeLevel() noexcept { return fadeLevel_; }
	const UINT8& fadeLevel() const noexcept { return fadeLevel_; }
	UINT16& unblitX() noexcept { return unblitX_; }
	const UINT16& unblitX() const noexcept { return unblitX_; }
	UINT16& unblitY() noexcept { return unblitY_; }
	const UINT16& unblitY() const noexcept { return unblitY_; }
	UINT16& unblitWidth() noexcept { return unblitWidth_; }
	const UINT16& unblitWidth() const noexcept { return unblitWidth_; }
	UINT16& unblitHeight() noexcept { return unblitHeight_; }
	const UINT16& unblitHeight() const noexcept { return unblitHeight_; }
	INT32& lightSprite() noexcept { return lightSprite_; }
	const INT32& lightSprite() const noexcept { return lightSprite_; }
	INT32& muzzleFlashSprite() noexcept { return muzzleFlashSprite_; }
	const INT32& muzzleFlashSprite() const noexcept { return muzzleFlashSprite_; }
	INT8& muzzleFlashFrame() noexcept { return muzzleFlashFrame_; }
	const INT8& muzzleFlashFrame() const noexcept { return muzzleFlashFrame_; }
	INT16& boundingBoxWidth() noexcept { return boundingBoxWidth_; }
	const INT16& boundingBoxWidth() const noexcept { return boundingBoxWidth_; }
	INT16& boundingBoxHeight() noexcept { return boundingBoxHeight_; }
	const INT16& boundingBoxHeight() const noexcept { return boundingBoxHeight_; }
	INT16& boundingBoxOffsetX() noexcept { return boundingBoxOffsetX_; }
	const INT16& boundingBoxOffsetX() const noexcept { return boundingBoxOffsetX_; }
	INT16& boundingBoxOffsetY() noexcept { return boundingBoxOffsetY_; }
	const INT16& boundingBoxOffsetY() const noexcept { return boundingBoxOffsetY_; }
	INT32& fadeOriginGrid() noexcept { return fadeOriginGrid_; }
	const INT32& fadeOriginGrid() const noexcept { return fadeOriginGrid_; }

	bool fading() const noexcept { return fadeMode_ != FALSE; }
	bool hasLightSprite() const noexcept { return lightSprite_ != NoLightSprite; }
	bool hasMuzzleFlashSprite() const noexcept { return muzzleFlashSprite_ != NoLightSprite; }
	bool muzzleFlashExpired(INT8 maximumFrame) const noexcept
	{
		return muzzleFlashFrame_ > maximumFrame;
	}
	void beginFade(UINT8 mode, UINT8 level, INT32 originGrid) noexcept
	{
		fadeMode_ = mode;
		fadeLevel_ = level;
		fadeOriginGrid_ = originGrid;
	}
	void finishFade() noexcept { fadeMode_ = FALSE; }
	void setUnblitRect(UINT16 x, UINT16 y, UINT16 width, UINT16 height) noexcept
	{
		unblitX_ = x;
		unblitY_ = y;
		unblitWidth_ = width;
		unblitHeight_ = height;
	}
	void setBoundingBox(INT16 width, INT16 height, INT16 offsetX, INT16 offsetY) noexcept
	{
		boundingBoxWidth_ = width;
		boundingBoxHeight_ = height;
		boundingBoxOffsetX_ = offsetX;
		boundingBoxOffsetY_ = offsetY;
	}
	void startMuzzleFlashSprite(INT32 sprite) noexcept
	{
		muzzleFlashSprite_ = sprite;
		muzzleFlashFrame_ = 1;
	}
	void advanceMuzzleFlashFrame() noexcept { ++muzzleFlashFrame_; }
	void clearMuzzleFlashSprite() noexcept
	{
		muzzleFlashSprite_ = NoLightSprite;
		muzzleFlashFrame_ = 0;
	}
	void showMuzzleFlash() noexcept { muzzleFlashVisible_ = TRUE; }
	void hideMuzzleFlash() noexcept { muzzleFlashVisible_ = FALSE; }
	void enableForceShade() noexcept { forceShade_ = TRUE; }
	void disableForceShade() noexcept { forceShade_ = FALSE; }
	void clearLightSprite() noexcept { lightSprite_ = NoLightSprite; }
	void reset() noexcept;

private:
	PaletteRepID headPalette_{};
	PaletteRepID pantsPalette_{};
	PaletteRepID vestPalette_{};
	PaletteRepID skinPalette_{};
	PaletteRepID miscPalette_{};
	UINT8 fadeMode_ = 0;
	BOOLEAN forceRenderColor_ = FALSE;
	BOOLEAN forceNoPaletteCycle_ = FALSE;
	BOOLEAN forceShade_ = FALSE;
	BOOLEAN muzzleFlashVisible_ = FALSE;
	UINT8 fadeLevel_ = 0;
	UINT16 unblitX_ = 0;
	UINT16 unblitY_ = 0;
	UINT16 unblitWidth_ = 0;
	UINT16 unblitHeight_ = 0;
	INT32 lightSprite_ = NoLightSprite;
	INT32 muzzleFlashSprite_ = NoLightSprite;
	INT8 muzzleFlashFrame_ = 0;
	INT16 boundingBoxWidth_ = 0;
	INT16 boundingBoxHeight_ = 0;
	INT16 boundingBoxOffsetX_ = 0;
	INT16 boundingBoxOffsetY_ = 0;
	INT32 fadeOriginGrid_ = 0;
};

// Canonical soldier-local tactical UI presentation state. Locator and portrait
// animation, panel placement and lifecycle, planned-action overlays, UI
// notifications, and enemy cycling are view state rather than simulation
// identity or render-resource ownership.
class SoldierUiPresentationComponent
{
public:
	INT8& portraitFlashFrame() noexcept { return portraitFlashFrame_; }
	const INT8& portraitFlashFrame() const noexcept { return portraitFlashFrame_; }
	INT16& locatorFrame() noexcept { return locatorFrame_; }
	const INT16& locatorFrame() const noexcept { return locatorFrame_; }
	INT16& locatorOffsetX() noexcept { return locatorOffsetX_; }
	const INT16& locatorOffsetX() const noexcept { return locatorOffsetX_; }
	INT16& locatorOffsetY() noexcept { return locatorOffsetY_; }
	const INT16& locatorOffsetY() const noexcept { return locatorOffsetY_; }
	INT8& interfaceLevel() noexcept { return interfaceLevel_; }
	const INT8& interfaceLevel() const noexcept { return interfaceLevel_; }
	UINT8& closePanelFrame() noexcept { return closePanelFrame_; }
	const UINT8& closePanelFrame() const noexcept { return closePanelFrame_; }
	UINT8& deadPanelFrame() noexcept { return deadPanelFrame_; }
	const UINT8& deadPanelFrame() const noexcept { return deadPanelFrame_; }
	INT8& openPanelFrame() noexcept { return openPanelFrame_; }
	const INT8& openPanelFrame() const noexcept { return openPanelFrame_; }
	INT16& panelFaceX() noexcept { return panelFaceX_; }
	const INT16& panelFaceX() const noexcept { return panelFaceX_; }
	INT16& panelFaceY() noexcept { return panelFaceY_; }
	const INT16& panelFaceY() const noexcept { return panelFaceY_; }
	UINT8& plannedActionPointCost() noexcept { return plannedActionPointCost_; }
	const UINT8& plannedActionPointCost() const noexcept { return plannedActionPointCost_; }
	INT16& plannedTargetX() noexcept { return plannedTargetX_; }
	const INT16& plannedTargetX() const noexcept { return plannedTargetX_; }
	INT16& plannedTargetY() noexcept { return plannedTargetY_; }
	const INT16& plannedTargetY() const noexcept { return plannedTargetY_; }
	SoldierID& lastEnemyCycled() noexcept { return lastEnemyCycled_; }
	const SoldierID& lastEnemyCycled() const noexcept { return lastEnemyCycled_; }
	UINT8& locateCycles() noexcept { return locateCycles_; }
	const UINT8& locateCycles() const noexcept { return locateCycles_; }
	BOOLEAN& panelCloseRequested() noexcept { return panelCloseRequested_; }
	const BOOLEAN& panelCloseRequested() const noexcept { return panelCloseRequested_; }
	BOOLEAN& panelCloseForDeath() noexcept { return panelCloseForDeath_; }
	const BOOLEAN& panelCloseForDeath() const noexcept { return panelCloseForDeath_; }
	BOOLEAN& deadPanelActive() noexcept { return deadPanelActive_; }
	const BOOLEAN& deadPanelActive() const noexcept { return deadPanelActive_; }
	BOOLEAN& panelOpenRequested() noexcept { return panelOpenRequested_; }
	const BOOLEAN& panelOpenRequested() const noexcept { return panelOpenRequested_; }
	BOOLEAN& locatorFlashCycle() noexcept { return locatorFlashCycle_; }
	const BOOLEAN& locatorFlashCycle() const noexcept { return locatorFlashCycle_; }
	BOOLEAN& locatorVisibleState() noexcept { return locatorVisible_; }
	const BOOLEAN& locatorVisibleState() const noexcept { return locatorVisible_; }
	BOOLEAN& portraitFlashPhase() noexcept { return portraitFlashPhase_; }
	const BOOLEAN& portraitFlashPhase() const noexcept { return portraitFlashPhase_; }
	BOOLEAN& deadMercUiPendingState() noexcept { return deadMercUiPending_; }
	const BOOLEAN& deadMercUiPendingState() const noexcept { return deadMercUiPending_; }
	BOOLEAN& newMercUiPendingState() noexcept { return newMercUiPending_; }
	const BOOLEAN& newMercUiPendingState() const noexcept { return newMercUiPending_; }
	BOOLEAN& closeMercUiPendingState() noexcept { return closeMercUiPending_; }
	const BOOLEAN& closeMercUiPendingState() const noexcept { return closeMercUiPending_; }
	BOOLEAN& firstNoActionPointsState() noexcept { return firstNoActionPoints_; }
	const BOOLEAN& firstNoActionPointsState() const noexcept { return firstNoActionPoints_; }
	BOOLEAN& firstUnconsciousState() noexcept { return firstUnconscious_; }
	const BOOLEAN& firstUnconsciousState() const noexcept { return firstUnconscious_; }

	bool panelClosing() const noexcept { return panelCloseRequested_ != FALSE; }
	bool panelClosingForDeath() const noexcept { return panelCloseForDeath_ != FALSE; }
	bool deadPanelShowing() const noexcept { return deadPanelActive_ != FALSE; }
	bool panelOpening() const noexcept { return panelOpenRequested_ != FALSE; }
	bool locatorFlashing() const noexcept { return locatorFlashCycle_ != FALSE; }
	bool locatorVisible() const noexcept { return locatorVisible_ != FALSE; }
	bool portraitFlashing() const noexcept { return portraitFlashPhase_ != FALSE; }
	bool deadMercUiPending() const noexcept { return deadMercUiPending_ != FALSE; }
	bool newMercUiPending() const noexcept { return newMercUiPending_ != FALSE; }
	bool closeMercUiPending() const noexcept { return closeMercUiPending_ != FALSE; }
	bool firstNoActionPoints() const noexcept { return firstNoActionPoints_ != FALSE; }
	bool firstUnconscious() const noexcept { return firstUnconscious_ != FALSE; }

	void setLocatorOffset(INT16 x, INT16 y) noexcept
	{
		locatorOffsetX_ = x;
		locatorOffsetY_ = y;
	}
	void startLocator(UINT8 cycles) noexcept
	{
		locatorFrame_ = 0;
		locateCycles_ = cycles;
		locatorFlashCycle_ = TRUE;
	}
	void advanceLocatorCycle() noexcept { ++locatorFlashCycle_; }
	void showLocator() noexcept { locatorVisible_ = TRUE; }
	void hideLocator() noexcept { locatorVisible_ = FALSE; }
	void stopLocatorFlash() noexcept { locatorFlashCycle_ = FALSE; }
	void stopLocator() noexcept
	{
		locatorFlashCycle_ = FALSE;
		locatorVisible_ = FALSE;
	}
	void setPanelFacePosition(INT16 x, INT16 y) noexcept
	{
		panelFaceX_ = x;
		panelFaceY_ = y;
	}
	void clearPanelAnimation() noexcept
	{
		closePanelFrame_ = 0;
		deadPanelFrame_ = 0;
		openPanelFrame_ = 0;
	}
	void requestPanelClose() noexcept
	{
		panelCloseRequested_ = TRUE;
		panelCloseForDeath_ = FALSE;
	}
	void beginDeathPanelTransition() noexcept
	{
		deadMercUiPending_ = FALSE;
		panelCloseRequested_ = TRUE;
		panelCloseForDeath_ = TRUE;
		closePanelFrame_ = 0;
		deadPanelFrame_ = 0;
	}
	bool completePanelClose(UINT8 finalFrame) noexcept
	{
		panelCloseRequested_ = FALSE;
		closePanelFrame_ = finalFrame;
		if (panelCloseForDeath_ != FALSE)
		{
			deadPanelActive_ = TRUE;
			return true;
		}
		return false;
	}
	void completeDeadPanel(UINT8 finalFrame) noexcept
	{
		deadPanelActive_ = FALSE;
		deadPanelFrame_ = finalFrame;
		panelCloseForDeath_ = FALSE;
	}
	void requestPanelOpen(INT8 startFrame) noexcept
	{
		panelOpenRequested_ = TRUE;
		openPanelFrame_ = startFrame;
	}
	void completePanelOpen() noexcept
	{
		panelOpenRequested_ = FALSE;
		openPanelFrame_ = 0;
	}
	void startPortraitFlash() noexcept { portraitFlashPhase_ = TRUE; }
	void setPortraitFlashPhase(BOOLEAN phase) noexcept { portraitFlashPhase_ = phase; }
	void stopPortraitFlash() noexcept { portraitFlashPhase_ = FALSE; }
	void queueDeadMercUi() noexcept { deadMercUiPending_ = TRUE; }
	void clearDeadMercUi() noexcept { deadMercUiPending_ = FALSE; }
	void queueNewMercUi() noexcept { newMercUiPending_ = TRUE; }
	void clearNewMercUi() noexcept { newMercUiPending_ = FALSE; }
	void queueCloseMercUi() noexcept { closeMercUiPending_ = TRUE; }
	void clearCloseMercUi() noexcept { closeMercUiPending_ = FALSE; }
	void finishDeathUi() noexcept
	{
		deadMercUiPending_ = FALSE;
		panelCloseForDeath_ = FALSE;
	}
	void markNoActionPoints() noexcept { firstNoActionPoints_ = TRUE; }
	void clearNoActionPoints() noexcept { firstNoActionPoints_ = FALSE; }
	void markUnconscious() noexcept { firstUnconscious_ = TRUE; }
	void clearUnconscious() noexcept { firstUnconscious_ = FALSE; }
	bool hasPlannedTarget() const noexcept { return plannedTargetX_ != -1; }
	void setPlannedTarget(INT16 x, INT16 y, UINT8 actionPointCost) noexcept
	{
		plannedTargetX_ = x;
		plannedTargetY_ = y;
		plannedActionPointCost_ = actionPointCost;
	}
	void clearPlannedTarget() noexcept
	{
		plannedTargetX_ = -1;
		plannedTargetY_ = -1;
	}
	void reset() noexcept;

private:
	INT8 portraitFlashFrame_ = 0;
	INT16 locatorFrame_ = 0;
	INT16 locatorOffsetX_ = 0;
	INT16 locatorOffsetY_ = 0;
	INT8 interfaceLevel_ = 0;
	UINT8 closePanelFrame_ = 0;
	UINT8 deadPanelFrame_ = 0;
	INT8 openPanelFrame_ = 0;
	INT16 panelFaceX_ = 0;
	INT16 panelFaceY_ = 0;
	UINT8 plannedActionPointCost_ = 0;
	INT16 plannedTargetX_ = 0;
	INT16 plannedTargetY_ = 0;
	SoldierID lastEnemyCycled_{};
	UINT8 locateCycles_ = 0;
	BOOLEAN panelCloseRequested_ = FALSE;
	BOOLEAN panelCloseForDeath_ = FALSE;
	BOOLEAN deadPanelActive_ = FALSE;
	BOOLEAN panelOpenRequested_ = FALSE;
	BOOLEAN locatorFlashCycle_ = FALSE;
	BOOLEAN locatorVisible_ = FALSE;
	BOOLEAN portraitFlashPhase_ = FALSE;
	BOOLEAN deadMercUiPending_ = FALSE;
	BOOLEAN newMercUiPending_ = FALSE;
	BOOLEAN closeMercUiPending_ = FALSE;
	BOOLEAN firstNoActionPoints_ = FALSE;
	BOOLEAN firstUnconscious_ = FALSE;
};

// Canonical requests that bridge tactical decisions into animation playback.
// The playback state itself remains separate: this component owns only queued
// animations, stance/facing intent, and the movement continuation policy that
// must survive until the requested transition completes.
class SoldierAnimationIntentComponent
{
public:
	UINT8& desiredHeight() noexcept { return desiredHeight_; }
	const UINT8& desiredHeight() const noexcept { return desiredHeight_; }
	UINT16& pendingAnimation() noexcept { return pendingAnimation_; }
	const UINT16& pendingAnimation() const noexcept { return pendingAnimation_; }
	UINT8& pendingStance() noexcept { return pendingStance_; }
	const UINT8& pendingStance() const noexcept { return pendingStance_; }
	UINT16& secondaryPendingAnimation() noexcept { return secondaryPendingAnimation_; }
	const UINT16& secondaryPendingAnimation() const noexcept { return secondaryPendingAnimation_; }
	UINT8& pendingDirection() noexcept { return pendingDirection_; }
	const UINT8& pendingDirection() const noexcept { return pendingDirection_; }
	INT8& turningFromUi() noexcept { return turningFromUi_; }
	const INT8& turningFromUi() const noexcept { return turningFromUi_; }
	BOOLEAN& stopPendingNextTile() noexcept { return stopPendingNextTile_; }
	const BOOLEAN& stopPendingNextTile() const noexcept { return stopPendingNextTile_; }
	UINT8& continuationMode() noexcept { return continuationMode_; }
	const UINT8& continuationMode() const noexcept { return continuationMode_; }

	bool hasDesiredHeight() const noexcept { return desiredHeight_ != NoDesiredHeight; }
	bool hasPendingAnimation() const noexcept { return pendingAnimation_ != NoPendingAnimation; }
	bool hasPendingStance() const noexcept { return pendingStance_ != NoPendingStance; }
	bool hasSecondaryPendingAnimation() const noexcept { return secondaryPendingAnimation_ != NoPendingAnimation; }
	bool hasPendingDirection() const noexcept { return pendingDirection_ != NoPendingDirection; }
	bool continuesAfterStance() const noexcept { return continuationMode_ != 0; }

	void requestHeight(UINT8 height) noexcept { desiredHeight_ = height; }
	void clearDesiredHeight() noexcept { desiredHeight_ = NoDesiredHeight; }
	void queueAnimation(UINT16 animation) noexcept { pendingAnimation_ = animation; }
	void clearPendingAnimation() noexcept { pendingAnimation_ = NoPendingAnimation; }
	void queueStance(UINT8 stance) noexcept { pendingStance_ = stance; }
	void clearPendingStance() noexcept { pendingStance_ = NoPendingStance; }
	void queueSecondaryAnimation(UINT16 animation) noexcept { secondaryPendingAnimation_ = animation; }
	void clearSecondaryPendingAnimation() noexcept { secondaryPendingAnimation_ = NoPendingAnimation; }
	void queueDirection(UINT8 direction) noexcept { pendingDirection_ = direction; }
	void clearPendingDirection() noexcept { pendingDirection_ = NoPendingDirection; }
	void queueFacingAnimation(UINT16 animation, UINT8 direction) noexcept;
	void clearFacingAnimation() noexcept;
	void markTurningFromUi() noexcept { turningFromUi_ = TRUE; }
	void clearTurningFromUi() noexcept { turningFromUi_ = FALSE; }
	void requestStopAtNextTile() noexcept { stopPendingNextTile_ = TRUE; }
	void clearStopAtNextTile() noexcept { stopPendingNextTile_ = FALSE; }
	void continueAfterStance(UINT8 mode = 1) noexcept { continuationMode_ = mode; }
	void clearContinuation() noexcept { continuationMode_ = 0; }
	void clearPendingAnimations() noexcept;
	void reset() noexcept;

private:
	static constexpr UINT8 NoDesiredHeight = 255;
	static constexpr UINT16 NoPendingAnimation = 32001;
	static constexpr UINT8 NoPendingStance = 254;
	static constexpr UINT8 NoPendingDirection = 253;

	UINT8 desiredHeight_ = NoDesiredHeight;
	UINT16 pendingAnimation_ = NoPendingAnimation;
	UINT8 pendingStance_ = NoPendingStance;
	UINT16 secondaryPendingAnimation_ = NoPendingAnimation;
	UINT8 pendingDirection_ = NoPendingDirection;
	INT8 turningFromUi_ = FALSE;
	BOOLEAN stopPendingNextTile_ = FALSE;
	UINT8 continuationMode_ = 0;
};

// Canonical state of the animation currently being played. Transition
// requests belong to SoldierAnimationIntentComponent; this component owns the
// frame cursor, timing, previous-state bookkeeping, and render selection that
// advance after a request has been accepted.
class SoldierAnimationPlaybackComponent
{
public:
	UINT16& state() noexcept { return state_; }
	const UINT16& state() const noexcept { return state_; }
	UINT16& code() noexcept { return code_; }
	const UINT16& code() const noexcept { return code_; }
	UINT16& frame() noexcept { return frame_; }
	const UINT16& frame() const noexcept { return frame_; }
	INT16& delay() noexcept { return delay_; }
	const INT16& delay() const noexcept { return delay_; }
	UINT16& previousState() noexcept { return previousState_; }
	const UINT16& previousState() const noexcept { return previousState_; }
	INT16& previousCode() noexcept { return previousCode_; }
	const INT16& previousCode() const noexcept { return previousCode_; }
	UINT16& surface() noexcept { return surface_; }
	const UINT16& surface() const noexcept { return surface_; }
	UINT16& zLevel() noexcept { return zLevel_; }
	const UINT16& zLevel() const noexcept { return zLevel_; }
	UINT32& subFlags() noexcept { return subFlags_; }
	const UINT32& subFlags() const noexcept { return subFlags_; }

	bool isPlaying(UINT16 animation) const noexcept { return state_ == animation; }
	void reset() noexcept;

private:
	UINT16 state_ = 0;
	UINT16 code_ = 0;
	UINT16 frame_ = 0;
	INT16 delay_ = 0;
	UINT16 previousState_ = 0;
	INT16 previousCode_ = 0;
	UINT16 surface_ = 0;
	UINT16 zLevel_ = 0;
	UINT32 subFlags_ = 0;
};

// Runtime lifecycle surrounding accepted animation playback. Modes that must
// coordinate turning, hit/fall completion, interruption, and one-shot AP
// charging live here rather than in the generic soldier flag bucket.
class SoldierAnimationActivityComponent
{
public:
	INT8& turningFromProneMode() noexcept { return turningFromProneMode_; }
	const INT8& turningFromProneMode() const noexcept { return turningFromProneMode_; }
	BOOLEAN& readyCostWaived() noexcept { return readyCostWaived_; }
	const BOOLEAN& readyCostWaived() const noexcept { return readyCostWaived_; }
	INT8& postHitStance() noexcept { return postHitStance_; }
	const INT8& postHitStance() const noexcept { return postHitStance_; }
	BOOLEAN& paused() noexcept { return paused_; }
	const BOOLEAN& paused() const noexcept { return paused_; }
	BOOLEAN& holdAttackerUntilDone() noexcept { return holdAttackerUntilDone_; }
	const BOOLEAN& holdAttackerUntilDone() const noexcept { return holdAttackerUntilDone_; }
	BOOLEAN& turningToShoot() noexcept { return turningToShoot_; }
	const BOOLEAN& turningToShoot() const noexcept { return turningToShoot_; }
	BOOLEAN& turningToFall() noexcept { return turningToFall_; }
	const BOOLEAN& turningToFall() const noexcept { return turningToFall_; }
	BOOLEAN& turningUntilDone() noexcept { return turningUntilDone_; }
	const BOOLEAN& turningUntilDone() const noexcept { return turningUntilDone_; }
	UINT8& hitPhase() noexcept { return hitPhase_; }
	const UINT8& hitPhase() const noexcept { return hitPhase_; }
	BOOLEAN& nonInterruptible() noexcept { return nonInterruptible_; }
	const BOOLEAN& nonInterruptible() const noexcept { return nonInterruptible_; }
	BOOLEAN& turningCostWaived() noexcept { return turningCostWaived_; }
	const BOOLEAN& turningCostWaived() const noexcept { return turningCostWaived_; }
	BOOLEAN& suppressionStanceChange() noexcept { return suppressionStanceChange_; }
	const BOOLEAN& suppressionStanceChange() const noexcept { return suppressionStanceChange_; }
	BOOLEAN& stanceCostWaived() noexcept { return stanceCostWaived_; }
	const BOOLEAN& stanceCostWaived() const noexcept { return stanceCostWaived_; }
	BOOLEAN& realtimeNonInterruptible() noexcept { return realtimeNonInterruptible_; }
	const BOOLEAN& realtimeNonInterruptible() const noexcept { return realtimeNonInterruptible_; }
	INT8& tryingToFall() noexcept { return tryingToFall_; }
	const INT8& tryingToFall() const noexcept { return tryingToFall_; }
	BOOLEAN& fallClockwise() noexcept { return fallClockwise_; }
	const BOOLEAN& fallClockwise() const noexcept { return fallClockwise_; }
	INT8& fallDirection() noexcept { return fallDirection_; }
	const INT8& fallDirection() const noexcept { return fallDirection_; }
	INT8& turningIncrement() noexcept { return turningIncrement_; }
	const INT8& turningIncrement() const noexcept { return turningIncrement_; }
	INT32& traversalForecastGrid() noexcept { return traversalForecastGrid_; }
	const INT32& traversalForecastGrid() const noexcept { return traversalForecastGrid_; }
	INT16& renderZOverride() noexcept { return renderZOverride_; }
	const INT16& renderZOverride() const noexcept { return renderZOverride_; }
	UINT32& randomActionCheckCounter() noexcept { return randomActionCheckCounter_; }
	const UINT32& randomActionCheckCounter() const noexcept { return randomActionCheckCounter_; }
	INT16& lastRandomAnimation() noexcept { return lastRandomAnimation_; }
	const INT16& lastRandomAnimation() const noexcept { return lastRandomAnimation_; }
	BOOLEAN& reactingFromShot() noexcept { return reactingFromShot_; }
	const BOOLEAN& reactingFromShot() const noexcept { return reactingFromShot_; }
	BOOLEAN& externalDeath() noexcept { return externalDeath_; }
	const BOOLEAN& externalDeath() const noexcept { return externalDeath_; }

	bool gettingHit() const noexcept { return hitPhase_ != 0; }
	bool hasRenderZOverride() const noexcept { return renderZOverride_ != -1; }
	bool randomActionCheckDue(UINT32 threshold) const noexcept
	{
		return randomActionCheckCounter_ > threshold;
	}
	void beginHit() noexcept { hitPhase_ = 1; }
	void advanceHit() noexcept { hitPhase_ = 2; }
	void clearHit() noexcept { hitPhase_ = 0; }
	void pause() noexcept { paused_ = TRUE; }
	void resume() noexcept { paused_ = FALSE; }
	void setInterruptibility(BOOLEAN nonInterruptible, BOOLEAN realtimeNonInterruptible) noexcept;
	void clearInterruptibility() noexcept;
	void beginFall(INT8 direction) noexcept;
	void clearFall() noexcept { tryingToFall_ = FALSE; }
	void forecastTraversalAt(INT32 grid) noexcept { traversalForecastGrid_ = grid; }
	void setRenderZOverride(INT16 zLevel) noexcept { renderZOverride_ = zLevel; }
	void clearRenderZOverride() noexcept { renderZOverride_ = -1; }
	void advanceRandomActionCheck() noexcept { ++randomActionCheckCounter_; }
	void resetRandomActionCheck() noexcept { randomActionCheckCounter_ = 0; }
	void reset() noexcept;

private:
	INT8 turningFromProneMode_ = 0;
	BOOLEAN readyCostWaived_ = FALSE;
	INT8 postHitStance_ = 0;
	BOOLEAN paused_ = FALSE;
	BOOLEAN holdAttackerUntilDone_ = FALSE;
	BOOLEAN turningToShoot_ = FALSE;
	BOOLEAN turningToFall_ = FALSE;
	BOOLEAN turningUntilDone_ = FALSE;
	UINT8 hitPhase_ = 0;
	BOOLEAN nonInterruptible_ = FALSE;
	BOOLEAN turningCostWaived_ = FALSE;
	BOOLEAN suppressionStanceChange_ = FALSE;
	BOOLEAN stanceCostWaived_ = FALSE;
	BOOLEAN realtimeNonInterruptible_ = FALSE;
	INT8 tryingToFall_ = FALSE;
	BOOLEAN fallClockwise_ = FALSE;
	INT8 fallDirection_ = 0;
	INT8 turningIncrement_ = 0;
	INT32 traversalForecastGrid_ = 0;
	INT16 renderZOverride_ = 0;
	UINT32 randomActionCheckCounter_ = 0;
	INT16 lastRandomAnimation_ = 0;
	BOOLEAN reactingFromShot_ = FALSE;
	BOOLEAN externalDeath_ = FALSE;
};

struct SoldierPendingActionRuntimeState
{
	// Debug/path scratch retained across the path-cost operation only.
	INT32 pathSearchSourceGrid = 0;

	// Incarnation paired with a legacy pending target slot/grid. Delayed
	// completion must not follow a slot after it has been reused.
	UINT32 targetIncarnation = 0;

	// Transient launcher selection and deferred damage work.
	UINT16 grenadeItem = 0;
	std::function<void()> delayedDamage;

	void reset() noexcept
	{
		pathSearchSourceGrid = 0;
		targetIncarnation = 0;
		grenadeItem = 0;
		delayedDamage = nullptr;
	}
};

struct SoldierCombatFeedbackState
{
	// Presentation counters for the most recent attack. They are intentionally
	// runtime-only and are never part of a soldier save payload.
	UINT8 lastShock = 0;
	UINT8 lastSuppression = 0;
	UINT8 lastActionPoints = 0;
	UINT8 lastMorale = 0;
	UINT8 lastShockFromHit = 0;
	UINT8 lastActionPointsFromHit = 0;
	UINT8 lastMoraleFromHit = 0;
	UINT8 lastBulletImpact = 0;
	UINT8 lastArmourProtection = 0;

	void reset() noexcept
	{
		*this = SoldierCombatFeedbackState{};
	}
};

struct SoldierQuickItemRuntimeState
{
	UINT16 itemId = 0;
	UINT8 slot = 0;

	void reset() noexcept
	{
		*this = SoldierQuickItemRuntimeState{};
	}
};

struct SoldierRuntimeComponents
{
	SoldierPendingActionRuntimeState pendingAction;
	SoldierCombatFeedbackState combatFeedback;
	SoldierQuickItemRuntimeState quickItem;

	SoldierRuntimeComponents() = default;

	// A SOLDIERTYPE clone represents a new runtime object. Never copy deferred
	// callbacks that capture the source soldier, stale target incarnations, or
	// presentation/UI scratch into that clone.
	SoldierRuntimeComponents(const SoldierRuntimeComponents&) noexcept {}

	SoldierRuntimeComponents& operator=(const SoldierRuntimeComponents&) noexcept
	{
		reset();
		return *this;
	}

	void reset() noexcept
	{
		pendingAction.reset();
		combatFeedback.reset();
		quickItem.reset();
	}
};

#endif
