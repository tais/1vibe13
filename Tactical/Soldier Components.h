#ifndef TACTICAL_SOLDIER_COMPONENTS_H
#define TACTICAL_SOLDIER_COMPONENTS_H

#include "types.h"

class SOLDIERTYPE;

// Focused views provide domain seams without moving fields or changing the
// serialized SOLDIERTYPE layout. They can become owned components once legacy
// saves and mod APIs have versioned migration paths.
class SoldierVitalsComponent
{
public:
	explicit SoldierVitalsComponent(SOLDIERTYPE& soldier) : soldier_(soldier) {}

	INT8& health();
	const INT8& health() const;
	INT8& maximumHealth();
	const INT8& maximumHealth() const;
	INT8& breath();
	const INT8& breath() const;
	INT8& maximumBreath();
	const INT8& maximumBreath() const;
	INT8& bleeding();
	const INT8& bleeding() const;
	bool alive() const;
	void applyLifeDeduction(INT16 lifeDeduction);

private:
	SOLDIERTYPE& soldier_;
};

class SoldierPositionComponent
{
public:
	explicit SoldierPositionComponent(SOLDIERTYPE& soldier) : soldier_(soldier) {}

	INT32& gridNo();
	const INT32& gridNo() const;
	INT8& level();
	const INT8& level() const;
	UINT8& direction();
	const UINT8& direction() const;

private:
	SOLDIERTYPE& soldier_;
};

#endif
