#include "TacticalActor.h"
#include "TacticalActorProfileClassification.h"

#include "Interface.h"
#include "MilitiaIndividual.h"
#include "Soldier Profile.h"
#include "Text.h"

#include <cwchar>

TacticalActor::~TacticalActor( ) = default;

TacticalActor::TacticalActor( ) {
	initialize( );
}

// Initialize the soldier.
// The constructor does this automatically; callers may reuse a record by
// explicitly resetting every owned component through this routine.
void TacticalActor::initialize( )
{
	// On a reused live record, release its slot-indexed surface locks before the
	// identity component is cleared. A brand-new actor has an empty cache, so
	// its id is never read before initialization.
	if ( !animationCache().empty() )
	{
		animationCache().release( identity().id() );
	}

	identity().reset();
	roster().reset();
	vitals().reset();
	statistics().reset();
	status().reset();
	featureFlags().reset();
	inventory().reset();
	keyRing().reset();
	pendingItem().reset();
	service().reset();
	dialogue().reset();
	audio().reset();
	replication().reset();
	movementMetrics().reset();
	aiPlan().reset();
	aiPlanning().reset();
	aiBehavior().reset();
	aiCommunication().reset();
	morale().reset();
	skillState().reset();
	condition().reset();
	drugState().reset();
	statProgress().reset();
	timing().reset();
	longAction().reset();
	interaction().reset();
	pendingAction().reset();
	actionPoints().reset();
	collapseState().reset();
	perception().reset();
	awareness().reset();
	camouflage().reset();
	employment().reset();
	assignment().reset();
	deployment().reset();
	strategicPath().reset();
	vehicleState().reset();
	schedule().reset();
	position().reset();
	frontArc().reset();
	movementHistory().reset();
	pathing().reset();
	movement().reset();
	turnState().reset();
	targeting().reset();
	attackSelection().reset();
	meleeApproach().reset();
	fireControl().reset();
	combatResult().reset();
	combatContribution().reset();
	suppression().reset();
	damageDisplay().reset();
	palette().reset();
	renderState().reset();
	uiPresentation().reset();
	animationIntent().reset();
	animationPlayback().reset();
	animationActivity().reset();
	animationCache().reset();
	renderBindings().reset();
	runtime().reset();

	// Initialize all SoldierID fields to NOBODY. 0 is a valid value!
	this->identity().id() = NOBODY;
	this->targeting().clearEngagedOpponent();
	this->targeting().clearLineOfFireTarget();
}



UINT8 tmpuser = 0;
static CHAR16	tmpname[2][MAX_ENEMY_NAMES_CHARS];	// we need 2 arrays, in case we need 2 name pointers in one string
STR16 TacticalActor::GetName( )
{
	++tmpuser;
	if ( tmpuser > 1 )
		tmpuser = 0;

	tmpname[tmpuser][0] = '\0';
	wcscat( tmpname[tmpuser], this->identity().name() );

	MILITIA militia;
	if ( GetMilitia( this->identity().individualMilitiaId(), &militia ) )
	{
		return militia.GetName( );
	}

	if ( this->identity().dataProfile() )
	{
		const INT8 type =
			TacticalActorProfileClassification::profileTableIndex(
				*this,
				this->roster().team());
		if ( type > -1 )
		{
			wcscpy( tmpname[tmpuser], zSoldierProfile[type][this->identity().dataProfile()].szName );
			tmpname[tmpuser][MAX_ENEMY_NAMES_CHARS - 1] = '\0';
		}
	}

	return tmpname[tmpuser];
}
