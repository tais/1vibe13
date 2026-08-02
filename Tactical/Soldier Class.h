#ifndef SOLDIER_CLASS_H
#define SOLDIER_CLASS_H

// Stable soldier classification codes used by tactical, strategic, inventory,
// editor, and profile-selection policy. Keep the established ordinal values:
// they are consumed by existing content tables and save serialization.
enum
{
	SOLDIER_CLASS_NONE,
	SOLDIER_CLASS_ADMINISTRATOR,
	SOLDIER_CLASS_ELITE,
	SOLDIER_CLASS_ARMY,
	SOLDIER_CLASS_GREEN_MILITIA,
	SOLDIER_CLASS_REG_MILITIA,
	SOLDIER_CLASS_ELITE_MILITIA,
	SOLDIER_CLASS_CREATURE,
	SOLDIER_CLASS_MINER,
	SOLDIER_CLASS_ZOMBIE,
	SOLDIER_CLASS_TANK,
	SOLDIER_CLASS_JEEP,
	SOLDIER_CLASS_BANDIT,
	SOLDIER_CLASS_ROBOT,
	SOLDIER_CLASS_MAX,
};

// Separate equipment choices exist for every human enemy/militia class before
// the creature class in the established tables.
#define SOLDIER_GUN_CHOICE_SELECTIONS SOLDIER_CLASS_CREATURE

#define SOLDIER_CLASS_ENEMY(bSoldierClass) \
	((bSoldierClass >= SOLDIER_CLASS_ADMINISTRATOR) && \
	 (bSoldierClass <= SOLDIER_CLASS_ARMY))
#define SOLDIER_CLASS_MILITIA(bSoldierClass) \
	((bSoldierClass >= SOLDIER_CLASS_GREEN_MILITIA) && \
	 (bSoldierClass <= SOLDIER_CLASS_ELITE_MILITIA))

#endif
