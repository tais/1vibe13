#ifndef TACTICAL_ACTOR_H
#define TACTICAL_ACTOR_H

#include "Animation Cache.h"
#include "Render Palette Bank.h"
#include "Soldier Components.h"
#include "Soldier Inventory.h"

class TacticalActor
{
public:
	TacticalActor();
	~TacticalActor();

	// Reset every owned component. The constructor does this automatically.
	void initialize();
	SoldierIdentityComponent& identity() noexcept { return identity_; }
	const SoldierIdentityComponent& identity() const noexcept { return identity_; }
	SoldierRosterComponent& roster() noexcept { return roster_; }
	const SoldierRosterComponent& roster() const noexcept { return roster_; }
	SoldierVitalsComponent& vitals() noexcept { return vitals_; }
	const SoldierVitalsComponent& vitals() const noexcept { return vitals_; }
	SoldierStatisticsComponent& statistics() noexcept { return statistics_; }
	const SoldierStatisticsComponent& statistics() const noexcept { return statistics_; }
	SoldierStatusComponent& status() noexcept { return status_; }
	const SoldierStatusComponent& status() const noexcept { return status_; }
	SoldierFeatureFlagsComponent& featureFlags() noexcept { return featureFlags_; }
	const SoldierFeatureFlagsComponent& featureFlags() const noexcept { return featureFlags_; }
	SoldierInventory& inventory() noexcept { return inventory_; }
	const SoldierInventory& inventory() const noexcept { return inventory_; }
	SoldierKeyRingComponent& keyRing() noexcept { return keyRing_; }
	const SoldierKeyRingComponent& keyRing() const noexcept { return keyRing_; }
	SoldierPendingItemComponent& pendingItem() noexcept { return pendingItem_; }
	const SoldierPendingItemComponent& pendingItem() const noexcept { return pendingItem_; }
	SoldierServiceComponent& service() noexcept { return service_; }
	const SoldierServiceComponent& service() const noexcept { return service_; }
	SoldierDialogueComponent& dialogue() noexcept { return dialogue_; }
	const SoldierDialogueComponent& dialogue() const noexcept { return dialogue_; }
	SoldierAudioComponent& audio() noexcept { return audio_; }
	const SoldierAudioComponent& audio() const noexcept { return audio_; }
	SoldierReplicationComponent& replication() noexcept { return replication_; }
	const SoldierReplicationComponent& replication() const noexcept { return replication_; }
	SoldierMovementMetricsComponent& movementMetrics() noexcept { return movementMetrics_; }
	const SoldierMovementMetricsComponent& movementMetrics() const noexcept { return movementMetrics_; }
	SoldierAiPlanningComponent& aiPlanning() noexcept { return aiPlanning_; }
	const SoldierAiPlanningComponent& aiPlanning() const noexcept { return aiPlanning_; }
	SoldierAiPlanComponent& aiPlan() noexcept { return aiPlan_; }
	const SoldierAiPlanComponent& aiPlan() const noexcept { return aiPlan_; }
	SoldierAiBehaviorComponent& aiBehavior() noexcept { return aiBehavior_; }
	const SoldierAiBehaviorComponent& aiBehavior() const noexcept { return aiBehavior_; }
	SoldierAiCommunicationComponent& aiCommunication() noexcept { return aiCommunication_; }
	const SoldierAiCommunicationComponent& aiCommunication() const noexcept { return aiCommunication_; }
	SoldierMoraleComponent& morale() noexcept { return morale_; }
	const SoldierMoraleComponent& morale() const noexcept { return morale_; }
	SoldierSkillStateComponent& skillState() noexcept { return skillState_; }
	const SoldierSkillStateComponent& skillState() const noexcept { return skillState_; }
	SoldierConditionComponent& condition() noexcept { return condition_; }
	const SoldierConditionComponent& condition() const noexcept { return condition_; }
	SoldierDrugStateComponent& drugState() noexcept { return drugState_; }
	const SoldierDrugStateComponent& drugState() const noexcept { return drugState_; }
	SoldierStatProgressComponent& statProgress() noexcept { return statProgress_; }
	const SoldierStatProgressComponent& statProgress() const noexcept { return statProgress_; }
	SoldierTimingComponent& timing() noexcept { return timing_; }
	const SoldierTimingComponent& timing() const noexcept { return timing_; }
	SoldierLongActionComponent& longAction() noexcept { return longAction_; }
	const SoldierLongActionComponent& longAction() const noexcept { return longAction_; }
	SoldierInteractionComponent& interaction() noexcept { return interaction_; }
	const SoldierInteractionComponent& interaction() const noexcept { return interaction_; }
	SoldierPendingActionComponent& pendingAction() noexcept { return pendingAction_; }
	const SoldierPendingActionComponent& pendingAction() const noexcept { return pendingAction_; }
	SoldierActionPointComponent& actionPoints() noexcept { return actionPoints_; }
	const SoldierActionPointComponent& actionPoints() const noexcept { return actionPoints_; }
	SoldierCollapseComponent& collapseState() noexcept { return collapseState_; }
	const SoldierCollapseComponent& collapseState() const noexcept { return collapseState_; }
	SoldierPerceptionComponent& perception() noexcept { return perception_; }
	const SoldierPerceptionComponent& perception() const noexcept { return perception_; }
	SoldierAwarenessComponent& awareness() noexcept { return awareness_; }
	const SoldierAwarenessComponent& awareness() const noexcept { return awareness_; }
	SoldierCamouflageComponent& camouflage() noexcept { return camouflage_; }
	const SoldierCamouflageComponent& camouflage() const noexcept { return camouflage_; }
	SoldierEmploymentComponent& employment() noexcept { return employment_; }
	const SoldierEmploymentComponent& employment() const noexcept { return employment_; }
	SoldierAssignmentComponent& assignment() noexcept { return assignment_; }
	const SoldierAssignmentComponent& assignment() const noexcept { return assignment_; }
	SoldierDeploymentComponent& deployment() noexcept { return deployment_; }
	const SoldierDeploymentComponent& deployment() const noexcept { return deployment_; }
	SoldierStrategicPathComponent& strategicPath() noexcept { return strategicPath_; }
	const SoldierStrategicPathComponent& strategicPath() const noexcept { return strategicPath_; }
	SoldierVehicleStateComponent& vehicleState() noexcept { return vehicleState_; }
	const SoldierVehicleStateComponent& vehicleState() const noexcept { return vehicleState_; }
	SoldierScheduleComponent& schedule() noexcept { return schedule_; }
	const SoldierScheduleComponent& schedule() const noexcept { return schedule_; }
	SoldierPositionComponent& position() noexcept { return position_; }
	const SoldierPositionComponent& position() const noexcept { return position_; }
	SoldierFrontArcComponent& frontArc() noexcept { return frontArc_; }
	const SoldierFrontArcComponent& frontArc() const noexcept { return frontArc_; }
	SoldierMovementHistoryComponent& movementHistory() noexcept { return movementHistory_; }
	const SoldierMovementHistoryComponent& movementHistory() const noexcept { return movementHistory_; }
	SoldierPathingComponent& pathing() noexcept { return pathing_; }
	const SoldierPathingComponent& pathing() const noexcept { return pathing_; }
	SoldierMovementComponent& movement() noexcept { return movement_; }
	const SoldierMovementComponent& movement() const noexcept { return movement_; }
	SoldierTurnStateComponent& turnState() noexcept { return turnState_; }
	const SoldierTurnStateComponent& turnState() const noexcept { return turnState_; }
	SoldierTargetingComponent& targeting() noexcept { return targeting_; }
	const SoldierTargetingComponent& targeting() const noexcept { return targeting_; }
	SoldierAttackSelectionComponent& attackSelection() noexcept { return attackSelection_; }
	const SoldierAttackSelectionComponent& attackSelection() const noexcept { return attackSelection_; }
	SoldierMeleeApproachComponent& meleeApproach() noexcept { return meleeApproach_; }
	const SoldierMeleeApproachComponent& meleeApproach() const noexcept { return meleeApproach_; }
	SoldierFireControlComponent& fireControl() noexcept { return fireControl_; }
	const SoldierFireControlComponent& fireControl() const noexcept { return fireControl_; }
	SoldierCombatResultComponent& combatResult() noexcept { return combatResult_; }
	const SoldierCombatResultComponent& combatResult() const noexcept { return combatResult_; }
	SoldierCombatContributionComponent& combatContribution() noexcept { return combatContribution_; }
	const SoldierCombatContributionComponent& combatContribution() const noexcept { return combatContribution_; }
	SoldierSuppressionComponent& suppression() noexcept { return suppression_; }
	const SoldierSuppressionComponent& suppression() const noexcept { return suppression_; }
	SoldierDamageDisplayComponent& damageDisplay() noexcept { return damageDisplay_; }
	const SoldierDamageDisplayComponent& damageDisplay() const noexcept { return damageDisplay_; }
	RenderPaletteBank& palette() noexcept { return palette_; }
	const RenderPaletteBank& palette() const noexcept { return palette_; }
	SoldierRenderStateComponent& renderState() noexcept { return renderState_; }
	const SoldierRenderStateComponent& renderState() const noexcept { return renderState_; }
	SoldierUiPresentationComponent& uiPresentation() noexcept { return uiPresentation_; }
	const SoldierUiPresentationComponent& uiPresentation() const noexcept { return uiPresentation_; }
	SoldierAnimationIntentComponent& animationIntent() noexcept { return animationIntent_; }
	const SoldierAnimationIntentComponent& animationIntent() const noexcept { return animationIntent_; }
	SoldierAnimationPlaybackComponent& animationPlayback() noexcept { return animationPlayback_; }
	const SoldierAnimationPlaybackComponent& animationPlayback() const noexcept { return animationPlayback_; }
	SoldierAnimationActivityComponent& animationActivity() noexcept { return animationActivity_; }
	const SoldierAnimationActivityComponent& animationActivity() const noexcept { return animationActivity_; }
	SoldierAnimationCacheComponent& animationCache() noexcept { return animationCache_; }
	const SoldierAnimationCacheComponent& animationCache() const noexcept { return animationCache_; }
	SoldierRenderBindingsComponent& renderBindings() noexcept { return renderBindings_; }
	const SoldierRenderBindingsComponent& renderBindings() const noexcept { return renderBindings_; }
	SoldierRuntimeComponents& runtime() noexcept { return runtime_; }
	const SoldierRuntimeComponents& runtime() const noexcept { return runtime_; }

	// Compatibility name lookup; new behavior belongs in a focused actor domain.
	STR16 GetName();

private:
	SoldierIdentityComponent identity_;
	SoldierRosterComponent roster_;
	SoldierVitalsComponent vitals_;
	SoldierStatisticsComponent statistics_;
	SoldierStatusComponent status_;
	SoldierFeatureFlagsComponent featureFlags_;
	SoldierInventory inventory_;
	SoldierKeyRingComponent keyRing_;
	SoldierPendingItemComponent pendingItem_;
	SoldierServiceComponent service_;
	SoldierDialogueComponent dialogue_;
	SoldierAudioComponent audio_;
	SoldierReplicationComponent replication_;
	SoldierMovementMetricsComponent movementMetrics_;
	SoldierAiPlanningComponent aiPlanning_;
	SoldierAiPlanComponent aiPlan_;
	SoldierAiBehaviorComponent aiBehavior_;
	SoldierAiCommunicationComponent aiCommunication_;
	SoldierMoraleComponent morale_;
	SoldierSkillStateComponent skillState_;
	SoldierConditionComponent condition_;
	SoldierDrugStateComponent drugState_;
	SoldierStatProgressComponent statProgress_;
	SoldierTimingComponent timing_;
	SoldierLongActionComponent longAction_;
	SoldierInteractionComponent interaction_;
	SoldierPendingActionComponent pendingAction_;
	SoldierActionPointComponent actionPoints_;
	SoldierCollapseComponent collapseState_;
	SoldierPerceptionComponent perception_;
	SoldierAwarenessComponent awareness_;
	SoldierCamouflageComponent camouflage_;
	SoldierEmploymentComponent employment_;
	SoldierAssignmentComponent assignment_;
	SoldierDeploymentComponent deployment_;
	SoldierStrategicPathComponent strategicPath_;
	SoldierVehicleStateComponent vehicleState_;
	SoldierScheduleComponent schedule_;
	SoldierPositionComponent position_;
	SoldierFrontArcComponent frontArc_;
	SoldierMovementHistoryComponent movementHistory_;
	SoldierPathingComponent pathing_;
	SoldierMovementComponent movement_;
	SoldierTurnStateComponent turnState_;
	SoldierTargetingComponent targeting_;
	SoldierAttackSelectionComponent attackSelection_;
	SoldierMeleeApproachComponent meleeApproach_;
	SoldierFireControlComponent fireControl_;
	SoldierCombatResultComponent combatResult_;
	SoldierCombatContributionComponent combatContribution_;
	SoldierSuppressionComponent suppression_;
	SoldierDamageDisplayComponent damageDisplay_;
	RenderPaletteBank palette_;
	SoldierRenderStateComponent renderState_;
	SoldierUiPresentationComponent uiPresentation_;
	SoldierAnimationIntentComponent animationIntent_;
	SoldierAnimationPlaybackComponent animationPlayback_;
	SoldierAnimationActivityComponent animationActivity_;
	SoldierAnimationCacheComponent animationCache_;
	SoldierRenderBindingsComponent renderBindings_;
	// Runtime-only state is grouped by behavior and reset as one boundary. It is
	// deliberately outside the serialized component field list.
	SoldierRuntimeComponents runtime_;
};

#endif
