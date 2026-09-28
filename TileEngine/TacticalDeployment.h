#ifndef JA2_TACTICAL_DEPLOYMENT_H
#define JA2_TACTICAL_DEPLOYMENT_H

// The native Spread action, without overhead-map widgets or a local click.
// Called only within an explicitly requested headless pre-battle world entry.
// Uses the same side fallback, RNG, sweet-spot placement and facing as the GUI.
// Failure after placement starts is not recoverable by replaying the operation.
bool SpreadHeadlessPreBattleMercs();

#endif
