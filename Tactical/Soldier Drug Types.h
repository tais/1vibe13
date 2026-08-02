#pragma once

// Stable indices and capacity for persistent drug effects. The unused slots
// are part of the established soldier save schema and remain serialized.
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
