#ifndef TACTICAL_SOLDIER_PATROL_TYPES_H
#define TACTICAL_SOLDIER_PATROL_TYPES_H

// Fixed patrol capacity from the established soldier save schema.
enum
{
	SOLDIER_PATROL_GRID_COUNT = 10,
};

// Legacy map-placement names. Keep these tied to the canonical actor-storage
// capacity so the in-memory and serialized layouts cannot drift apart.
#define OLD_MAXPATROLGRIDS SOLDIER_PATROL_GRID_COUNT
#define MAXPATROLGRIDS SOLDIER_PATROL_GRID_COUNT

#endif
