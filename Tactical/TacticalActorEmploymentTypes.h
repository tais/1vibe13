#pragma once

// Persistent SoldierEmploymentComponent classification. Keep the established
// order because save data and scripting expose these numeric values.
enum
{
	MERC_TYPE__PLAYER_CHARACTER,
	MERC_TYPE__AIM_MERC,
	MERC_TYPE__MERC,
	MERC_TYPE__NPC,
	MERC_TYPE__EPC,
	MERC_TYPE__NPC_WITH_UNEXTENDABLE_CONTRACT,
	MERC_TYPE__VEHICLE,
};

enum
{
	SOLDIER_CONTRACT_RENEW_QUOTE_NOT_USED = 0,
	SOLDIER_CONTRACT_RENEW_QUOTE_89_USED = 1,
	SOLDIER_CONTRACT_RENEW_QUOTE_115_USED = 2,
};
