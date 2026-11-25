# Server-authoritative co-op completion

This is the ordered implementation and acceptance checklist for playable co-op,
not a declaration that a milestone, alpha, or the complete campaign is finished.
This checklist tracks the requested implementation, not separate requests to
authorize each item.

The dedicated host owns gameplay. Clients present authorized state and submit
intent; enabling client-local tactical or strategic simulation is not a shortcut.
The existing legacy multiplayer path remains separate. PvP has **not** been
migrated to this authority model, and this work is not equivalent to existing
legacy multiplayer or proof that all possible desynchronization is eliminated.

## Current implementation (2026-09-28)

Native scene PR [#878](https://github.com/tais/1vibe13/pull/878), shared
scene/protocol PR [#879](https://github.com/tais/1vibe13/pull/879), and bounded
client callback PR [#881](https://github.com/tais/1vibe13/pull/881), server
listener PR [#883](https://github.com/tais/1vibe13/pull/883), native-arrival
PR [#884](https://github.com/tais/1vibe13/pull/884), and content-identity
PR [#885](https://github.com/tais/1vibe13/pull/885) have merged after
all 15 checks passed. #881 merged as
`4d4a06870a1fbf75942035c25f444d77b67614c0`; #883 merged as
`d73c472294bf3fef2f04471f2ee953a859dbdac8`; #885 merged as
`0d96882fc53e6f6a4754491cace0f969968a5967`. #884 merged after its
corrected Windows/Linux checks passed as
`588ef151b1441104ebae716d252d45f5c3e998cb`.

- [#880](https://github.com/tais/1vibe13/pull/880), `07bff6b84`, isolates the
  passive campaign core, current observations and exact request/receipt handling.
  Six focused checks pass after rebase; transport and controls remain separate.
  Its Windows real-socket fixture subsequently crashed without a symbolized
  trace. A local 512 KiB thread-stack diagnostic reproduces an overflow in
  coordinator startup; heap-owning the two large fixture coordinators reduces
  frames by 247,744 bytes each and makes that diagnostic pass. Normal socket
  coverage passes again. All 15 corrected-head checks, including Windows,
  passed before merge as `138a5abbf2d8d493566109af30b26a9f26579c17`.
- [#882](https://github.com/tais/1vibe13/pull/882), `805839525`, connects seven
  campaign observations/results and three request types to the passive client.
  Socket tests cover nonleader travel, hiring and scene Skip, leader-only time,
  duplicates, ordering, directions and widths. It is now based on development
  after #880 merged. Its complete final Debug build and six focused checks pass,
  including architecture (208.17 s), both bounded-callback tests and the corrected
  socket fixture. The transport also passes integrated strict ASan.
- [#883](https://github.com/tais/1vibe13/pull/883), `406e5f084`, adds authenticated
  server bindings with a coordinator opt-in for mutation requests. Disabled
  handlers reject the sender before an unsupported command reaches a sync-only
  coordinator. Four checks including architecture, isolated JA2 build, four
  integrated ASan checks and a guard-removal negative control pass.
- [#884](https://github.com/tais/1vibe13/pull/884), `91531bc99`, isolates native
  strategic time, adjacent on-foot travel and arrival decisions. Shared native
  prebattle preparation/deployment/retreat code preserves gameplay decisions;
  arrival choices remain explicit and unsupported choices remain pending. The
  event queue commits departure events atomically without changing existing
  node identities or equal-time FIFO order. Established paid AIM flights retain
  their ordinary native arrival path. The isolated application and 226 macOS
  CTests passed, as did 29 integrated ASan cases. Windows/Linux CI then found
  an FPE in the scout fixture: native morale reached campaign-progress divisors
  normally loaded from XML. The corrected fixture supplies all three inputs
  and asserts their nonzero native totals. All 19 arrival scenarios now pass
  normally and under ASan; three separate missing-input negative controls fail
  on ARM too. All 15 corrected-head CI checks passed before merge. Equal-time ordering
  and the independent arrival hold also retain passing negative controls.
- [#885](https://github.com/tais/1vibe13/pull/885), `422573756`, excludes only the
  exact root-level native diagnostic logs `Strategic Decisions.txt` and
  `DebugMessage.txt` from installed content identity. Classic asset archives
  contain historical copies which otherwise conflict with fresh writable logs.
  Gameplay shadowing and similarly named directories remain rejected/hashed.
  Normal, integrated ASan, architecture and two negative controls pass. This
  changes identity for manifests which previously included those log entries;
  mismatched existing manifests are not automatically trusted or migrated.

#880 and #882 have merged. All 15 checks passed on #882's final `805839525`
head before merge as `93e5b4121824f0ad52796f01f1fbfa156d8ff2fb`.

Server activation is now [PR #886](https://github.com/tais/1vibe13/pull/886),
head `216b5311d`, on current development. Its complete isolated build and all
226 CTests pass (191.25 s); the four runtime/application source files match the
matching installed qualification byte for byte. After #880/#882 merged, all 15
checks passed before #886 merged as `30fac932db6649f680fec389d546150d3f07c605`.

The server runtime integration originated as `50b4d8157` on preserved branch
`codex/campaign-runtime`, with prerequisite commits from #883, #884 and #885
and fixture correction `68c092186`. It publishes
current campaign observations, serializes time/action/hire requests, recaptures
observations after each mutation and orders them before terminal receipts.
Either ready player may act; only the time leader may control the clock. Native
scene/arrival/defeat holds keep networking alive without implicit choices. The
shipping Debug server builds; 225 non-architecture tests pass (143.51 s), and
the final architecture check passes (188.84 s). The passive client bindings and
native prerequisites merged before activation.

Installed qualification with that isolated Debug server and the integrated
normal client now passes: `ja2-coop-pending-hire-flight-y9zq30jv`. The nonleader
explicitly skips the first-battle scene, the server executes native consequences
while remaining paused until the leader resumes, and Grunty arrives at minute
2250 as actor `4:5`, group `30:30`, contract end 12300. Both clients agree on the
five-person roster and balance 13640 through save, cold Resume and reconnect.
All seven processes exit 0; installed inputs, source/private state and binaries
remain unchanged. Final generation 17 save SHA-256 is
`b9466ea2d0a00b25bfef648a99e72615166ec6e063e670e273647ff399626b55`.
This run is worldless and has a controlled-victory ancestor; it does not qualify
unseeded combat, two-player combat contribution, physical input or rendering.

Earlier isolated attempts exposed two real dependencies: #885's diagnostic-log
manifest fix and a legacy first-visit help dialog which prevented the map-screen
handler from reaching the shared scene. A read-only process sample established
`MapScreenHandle -> HelpScreenHandler`. The runtime activation now disables
local help dialogs on dedicated hosts without changing save-owned help bits.
The qualifier additionally waits for a coherent notice on both clients and
recaptures the held campaign before asserting stability or sending input.
Seven scene-helper tests and eight flight-helper tests pass. The readiness wait
alone did not fix the help-screen stall; the successful run includes both fixes.

Client campaign controls are now [PR #888](https://github.com/tais/1vibe13/pull/888),
head `6837bc1f2`, based on development in `native-opplist-expiry` on
`codex/shared-campaign-screen`. This slice adds live campaign status, leader time
controls, adjacent travel and explicit arrival/surrender/defeat/scene choices,
plus two-step AIM hire confirmation. Current revisions invalidate armed choices;
key repeats cannot submit again. It retains the existing passive tactical renderer.
The complete isolated Debug build and **228/228 CTests pass** (187.67 s including
architecture 187.66 s). All sixteen production/behavior-test files match the
previous qualified slice byte for byte; independent CMake/architecture/sanitizer
registrations were combined. ASan workflow coverage validates 160 executable
targets. Five prior integrated ASan UI policy tests and two negative controls
also pass. An additional architecture-only commit moves the identical checks
beside the passive-screen contracts, removing the demonstrated EOF conflict
with #886; architecture passes again (189.96 s), and simulated merges with the
runtime, wire and equipment branches are clean. After #882/#886 merged, all 15
checks passed before #888 merged as `181019f8852625dc1952db8255c9e6bc82c3b303`.
This is not physical-input or complete-rendering qualification.

The preserved combined branch `codex/campaign-integration`, head `27df29437`,
contains the runtime,
client core/wire/controls, bounded callbacks and both fixture corrections.
The complete Debug build and **228/228 CTests pass** (345.60 s, including
architecture 183.26 s). ASan target coverage, source linters and release-workflow
checks pass. This integration is local, not a merged development release.
A campaign-only opt-in probe was built against the same library set without
adding a shipping hook. Matching installed qualification now passes:
`ja2-coop-pending-hire-flight-02o2qo61` covers nonleader scene Skip, native
paid-hire arrival, save, cold Resume and reconnect. All seven processes exit 0;
installed inputs, ancestor/private state and binaries remain unchanged. Its
final generation-18 save SHA-256 is
`63c53654b3eeafa68fe7a10035a6a6e958f327294de566a6f04a5b0f7454434a`.
The matching `ja2-coop-hiring-33l__sz8` also passes: first Enter only arms the
nonleader's seven-day Grunty contract; confirmation debits $5500 exactly once,
creates one in-transit actor and converges both clients before a pending-hire
checkpoint. All three processes exit 0, and input/private state is unchanged.
Both native AIM screenshots retain identical offer and contract pixels; the
armed screen adds the correct confirmation. Separate input and completion
manifests record clean integration HEAD and unchanged application/probe hashes.
These captures qualify only the two shown framebuffer states. Seeded ancestry,
unseeded combat, two-player combat contribution, physical input and complete
rendering remain outside these passes. The desktop computer-use service cannot
start, so physical-input qualification is still open.

Checked native equipment construction is [PR #887](https://github.com/tais/1vibe13/pull/887),
merged after all 15 checks passed as `817b8e348799f0ca37bbdc92ceec4e834f6779b6`.
Native sorting runs on a private profile with quantity and
condition conservation, checked creation cannot omit failed/unplaced items,
and existing constructors retain their route. The isolated build and all 226
CTests pass (186.01 s), both native hiring tests pass under integrated ASan/UBSan
(1.96 s), and removing sorting conservation or checked construction independently
fails its native regression. Live gear offers remain disabled in that PR.

The next local slice is on `codex/campaign-hire-equipment` in `native-surrender`,
now head `b934e3c1b`, above checked-equipment integration `8c8ba17a7`. It includes
offers/controls `556955d2b`, ammunition regression `5d378f231` and the final
undefined-magazine guard `b934e3c1b`.
It offers validated original unpurchased kits, includes gear in the shared total,
and adds released-key G selection with fresh confirmation after any change.
Native payment/input tests pass normally (0.55 s) and under integrated ASan/UBSan
(1.15 s); architecture passes (182.29 s). The real native fixture delivers three
80-point medical kits and records the exact equipment/salary/deposit payment.
This is fixture evidence, not installed inventory or first-aid acceptance.
Installed two-client purchase/checkpoint qualification now passes as
`ja2-coop-hiring-2_6ozu3c` with a matching opt-in probe. The nonleader confirms
seven-day Grunty with equipment: salary $4000, deposit $1500 and gear $2115,
charged exactly once ($19140 to $11525). Both clients agree on actor `4:5`,
pending arrival 2250; all three processes exit 0. Input/source/private state
remains unchanged, and recorded binaries/scripts/checkpoint hashes were
rechecked. Final generation 4 save SHA-256 is
`d543b8be43a5565790238b15e54e08da9a6cd30402bd7f8ed4d19dd0f8238c62`.
Two native screenshots show the gear-inclusive price and confirmation; their
offer/price rows are byte-identical. Six negative oracle controls reject missing
gear, non-boolean flags and incorrect charges. The inherited final stdout label
says "no-gear"; the executed qualifier explicitly requires both gear flags true
and the full $7615 debit. The original log/script remain unchanged, and the
completion manifest records this label correction. Installed inventory contents,
arrival after this gear purchase, physical input and full rendering remain open.
Gear repurchase needs authoritative kit selection/repricing; paid kits are not
offered again for free.

Cold native Resume now independently passes as `ja2-coop-hiring-restore-g8nftcto`
using a separate read-only GameLoop observer linked against unchanged native
libraries from clean `556955d2b`. It reads the actual restored Grunty actor:
all nine original items retain their quantities and normal conditions; the
three spare magazines each retain their full fifteen rounds. Both clients
retain the paid pending hire and $11525 balance, and shutdown commits generation
7 with save SHA-256 `0a9fd8e9ac24f946384d192fa4fe602f0b9ccc9fe49f16a4de3dc8055f46be07`.
All three processes exit0; inputs/source/private state/binaries remain unchanged.
This is native observation with an instrumented server, not a shipping-only
server run. Loaded gun rounds, attachments, LBE contents, first-aid, physical
input and full rendering remain outside the proof. The earlier observer attempt
`i5m1jzm2` failed because it interpreted the ammo ObjectData union as item
condition; native CreateItem deliberately uses magazine capacity for ammo.
Its original files/evidence are retained. The corrected observer exports both
catalog capacity and actual rounds and passes eight negative oracle controls.

The gear slice additionally retains `5d378f231`'s real native transaction case:
three 80-point medical kits plus three fifteen-round magazines, full payment
and no duplicate charge. It passes isolated normally (0.56 s) and integrated
ASan/UBSan (1.07 s). A new malformed-magazine regression then demonstrated that
zero-capacity ammunition could be offered and charged while constructing an
empty stack. Checked preflight now rejects that catalog entry before creating
an actor or writing ledgers. The regression fails before the guard; both native
hiring suites pass after it (1.04 s normal, 2.03 s integrated ASan/UBSan).
The final shipping build and architecture pass on clean `b934e3c1b` (186.84 s).
Matching two-client purchase/checkpoint run `ja2-coop-hiring-671nv_ba` also passes:
one $7615 debit, both clients at $11525, one pending Grunty and clean shutdown.
All three exits are0; input/source/private state and recorded binaries remain
unchanged. Final save SHA-256 is
`58b52c6a38d442b7935b643a5f907ab82cf19a267ef6210f5b022233bea61605`.
Inputs/completion records are in `gear-final-01`; the preceding qualified
shipping binary was archived unchanged before this rebuild. The cold native
inventory proof above remains explicitly tied to `556955d2b`.


The normal AIM medical-supply route also passes on clean `b934e3c1b`.
`ja2-coop-hiring-cjn6k2os` hires Spider (profile19) with original equipment through
the nonleader's confirmed request: one $7783 debit leaves both clients at $11357,
with one pending actor `4:5`. Cold native Resume `ja2-coop-hiring-restore-fc9agafw`
uses a separate read-only observer and confirms all nine original items,
including a real Med Kit (item202) with100 supply points in slot14 and two full
six-round magazines. Both clients retain the paid actor, original four teammates,
contracts and funds. All six processes across the two runs exit0; installed
inputs, source/private state and recorded binaries/scripts remain unchanged.
Final generation7 save SHA-256 is `e835577e0d88364b11dd492c4074794c5255c8963405e848a5274800600aeb86`.
Five negative inventory oracles reject missing/spent kits, wrong catalog class,
unpaid gear and a substituted actor. The first medical restore helper stopped
before process launch because adapting a profile literal also changed its hash
width; the corrected helper validates the real source and nine fingerprints
before launching. Both versions and the failure log are retained. This closes
medical-kit purchase and cold persistence. Native flight arrival is now qualified
below; equipping through client inventory controls, treatment, physical input
and full rendering remain open.
Records: `medical-gear-01/qualification-completed.json` and `restore-completed.json`.

The equipment work is now split into two reviewable PRs. Server offers and the
zero-capacity magazine guard are [#889](https://github.com/tais/1vibe13/pull/889),
head `07dab6bff`, based on merged #887. All three files match the qualified
`b934e3c1b` integration. The complete isolated Debug build and 226 CTests pass
(185.77 s). The first full run had one history-write assertion in the existing
pending-hire death fixture; its cause remains unconfirmed and the failure log
is retained. Death/capture subsequently passed 20 runs each normally and 10
runs each under strict ASan/UBSan, followed by the successful full rerun.
No production suppression was introduced. All 15 CI checks passed before #889
merged as `a1e288c2ff981770ca5e569339ca20fbbeee5aab`.

Client equipment selection is [#890](https://github.com/tais/1vibe13/pull/890),
rebased from `839e538e0` to `573d0285b` on development after #888 merged. The
four changed files still match the qualified integration; range-diff confirms
the same single equipment commit. The prior application build and eight
focused/architecture checks passed (186.13 s). The rebased JA2 application and
all eight focused/architecture checks pass (189.93 s). All 15 CI checks passed
before merge as `0eca0c791af54ba3cba1b3ca8c7612e61cc6e0a6`.
Actual treatment remains open; the arrived kit is observed below.

Inventory codec foundation [#891](https://github.com/tais/1vibe13/pull/891),
head `5751c0d6f`, adds bounded typed owner inventory/ground summaries without
activating the stream. Four focused/architecture checks pass (188.47 s), as
does strict ASan/UBSan. A regression first proved that two incarnations of one
native ground slot were accepted; encoder and decoder now require strictly
increasing native slots. A separate padding-guard removal fails nine cases.
The corrected three source/test files were copied to the integrated checkout
only after verifying exact preimages and releasing the installed-run freeze.
The branch was rebased from `3cd54c30d` onto current development. The sole
conflict was the shared CI target line; inventory now has a separate build step
and preserves all campaign targets. All three codec/regression files remain
byte-identical. The rebuilt codec and architecture tests pass (189.19 s), and
ASan coverage validates 161 executable targets. All 15 exact-head CI checks
passed before merge as `aafef9ef3677c55ec786506157d87fe875f476ca`.
The superseded workflow was cancelled after the exact-head push.

The medical campaign's integrated battle-return attempt `ri9kemqz` passed
native victory history and unload/checkpoint, then failed cold-resume validation
because the full test client did not expose the gear-selection field required
by the paid-equipment observer. All six launched processes exited 0; 2254 source
files and eleven input/binary hashes remained unchanged. The failed evidence
is retained. The test client now reports the selection and binds it to AIM
input/capture, with strict typed validation; 651 helper tests and two focused
input-policy/codec checks pass. The complete rerun `_jdmbysl` now passes native
victory history, world retirement/checkpoint, shipping cold Resume and reconnect.
All seven processes exit 0, with all 2254 source and eleven input/binary hashes
unchanged. The paid Spider actor remains pending at minute 2250, alongside the
four original teammates and unchanged $11357 balance. Final generation 11 save
SHA-256 is `0e266bf7512d0e2c678654cf87c5d9bf9aa6e21da2a741a5428c733ebd71fa2e`.
The shipping-only native flight successor `ocr8bu3k` also passes: the nonleader
explicitly skips the first-battle scene, the leader resumes native campaign
time, and Spider arrives at minute 2250 as actor `4:5` in group `30:30`, with
contract end 12300 and balance $11357. Both clients retain that cohort through
save, cold Resume and reconnect. All seven processes exit 0; 2254 source and
eleven input/binary hashes remain unchanged. Final generation 15 save SHA-256
is `5468ddfbcef3f2e888e89a7a2a0858dac62ed6a120db36d9f992e32c610d66e1`.
This run uses the existing `JA2_VSYNC=0` presentation option for the dummy display;
the normal frame cap and native campaign-clock controls remain active. It does
not observe the arrived actor's private inventory contents. The battle ancestor
is still controlled, and unseeded combat, physical input, complete rendering and
first-aid treatment remain unqualified.

Shipping continuation `ja2-coop-medical-session-f2zrl8vv` now proves explicit
nonleader orders through A9→A10→A11→A12→B12→B13, with only the designated leader
advancing time. A separate A12→A13 request was rejected with
`NativeInteractionRequired` and left the squad in place. Both clients agree on
the five-person cohort, $11357 balance, native prepared B13 battle at minute
2744, and the nonleader's Applied Spread entry. The ordinary loader publishes
one tactical world with all five actors and disjoint 3/2 ownership; no enemies,
health, gear or events were injected in this continuation. Its ancestor remains
the controlled victory described above, and no combat outcome is qualified.

Spider's arrived private inventory retains the 100-point medical kit in pocket
14 and both six-round magazines. Queued shipping Tab/I/mouse controls select
Spider and the kit. One actual hand-slot click receives `GameplayRejected`
(command 1, reason 12); every private inventory remains unchanged. Native
inspection identifies the blocker: the otherwise ordinary, unchanged offhand
canteen is `IC_MISC`, excluded by the inventory object proof. The captured
frame shows Spider's real inventory and the kit's 100 points; it is one shown
frame, not physical-input or complete-paint qualification. No AID was attempted.
Both clients exit 0; the server exits 2 because active tactical worlds cannot
checkpoint. The last checkpoint remains generation 15 at minute 2250, so this
travel/entry is not save/resume proof. All frozen sources, binaries, installed
inputs and ancestor/private state remain unchanged through shutdown.

The resulting native regression fails on that exact canteen/kit preparation
before the fix. Plain miscellaneous objects now receive the same complete
ordinary-object proof while retaining trap, attachment, active-LBE/alternate
payload and exact metadata checks. World-control markers remain excluded even
if tagged as miscellaneous. Miscellaneous first-status values keep an unknown
metric instead of being mislabeled as condition or water points. Ten native
swap/transfer/pickup/drop checks pass normally (3.00 s) and under ASan
(4.77 s), including canteen stow/restore,
unchanged offhand conservation, stale offhand rejection and failure cases.
The architecture check passes (194.66 s); shipping binaries were relinked in
the existing build directory.

Installed continuation `ja2-coop-medical-session-9002vaqj` now verifies the
medical equip: after the same five-leg route and native Spread entry, queued
shipping Tab/I/mouse input moves Spider's 100-point kit from pocket 14 to hand
5. Command 1 is Applied and private revision advances 1→2. The existing swap
oracle verifies exact slot conservation, unchanged offhand canteen/ammunition,
and both clients' matching public hand. A screenshot confirms that shown frame.
An earlier capture request failed because replacement inventory cleared the
selected item; the operator verified that no input/capture occurred, selected
the new hand item, and captured once. The failed request remains in the journal.
This is real-time equip evidence, not combat AP, treatment, physical-input or
complete-rendering qualification.

That continuation later crashes with SIGSEGV during Barry's movement toward
the airport. The final move was accepted but never completed; no AID was sent.
The server exits -11, both clients exit 0, and all frozen source, binary, asset,
ancestry and private-state checks remain unchanged. Its last checkpoint is
still the original generation 15, so the route is not save/resume proof.
The native crash stack ends in `StateRegistry::handle`, which does not by
itself establish the original corrupting operation.

Sanitizer continuation `ja2-coop-medical-session-vmi3g6e_` repeats the same
shipping travel and Applied native entry with the existing strict-ASan server.
It stops at the first tactical frame: `ReduceStringLength` passes an assumed
1024-character capacity for `RenderTownIDString`'s actual 80-character buffer.
The formatter's final terminator writes outside that buffer into a retired
stack frame. This establishes a concrete memory-corruption bug; the relationship
to the later Debug crash still requires the corrected campaign run. The server
exits -6, clients exit 0, and all frozen inputs remain unchanged. Treatment and
the complete encounter remain unqualified.

The correction is [PR #892](https://github.com/tais/1vibe13/pull/892),
`54e1c3bd3`, developed in the reused, clean sector-label worktree on current
development. The native font/shortening regression fails before the fix and
passes afterward normally (0.62 s) and under strict ASan (1.15 s).
Arrays retain their actual capacity; pointer callers must supply it explicitly.
Shortening now copies only the bounded suffix in place, with no 1024-character
temporary, and rejects unterminated storage without mutation. Coverage includes
the actual 80-character caller, exact fit, tiny buffers, narrow pixel budgets,
literal percent signs, accented text, zero-width glyphs and 2000-character input.
The isolated shipping build and native/architecture checks pass (227.47 s),
as do source ratchets and sanitizer-target coverage. All 15 checks passed
before merge as `110e4e257fe1ba7c546600666de5f60f006b84d8`. The corrected installed
ASan replay has reached B13 with both tactical replicas ready beyond the former
first-frame failure. Barry also completes the exact movement leg that failed
in the previous Debug run; native radio animations continue without a sanitizer
error. The reused `build-asan-final` flags enable AddressSanitizer only; the
recorded `UBSAN_OPTIONS` does not establish UBSan instrumentation for these
latest native misc/word-wrap checks.

That replay, `2io7fx1d`, subsequently enters native combat. Spider reaches a
cardinal-adjacent tile with her real kit; Barry fires at an actually visible
hostile, and both clients record ammunition 6→5 and combat AP 77→55. A later
EndTurn and one serial-bound Spider interrupt pass are Applied, but the native
AI turn does not complete within 120 seconds or an additional 180-second
readiness observation. No EndTurn is replayed and no AID is submitted. The
server logs an AI move-to-climb deadlock; the compiled `JA2TESTVERSION` branch
sets a local debug-UI hold instead of invoking `EndAIDeadlock`. This is the next
reproduction/fix target; no live debugger flag value was obtained because the
OS denied attachment. The session closes with server exit 2 (active tactical
checkpoint unsupported), clients exit 0 and every frozen source, binary, asset,
ancestry and private-state check unchanged. The native combat shot is proven;
the turn cycle, treatment, combat outcome and tactical persistence are not.

[PR #893](https://github.com/tais/1vibe13/pull/893), `661b01267`, reproduces
that dedicated Debug hold through the actual native watchdog with two native
actors and a queued AI successor. Before the fix, three dedicated recovery
assertions fail while interactive Debug passes. Dedicated hosts now run the
existing `EndAIDeadlock` cancellation and actor handoff; interactive Debug keeps
its local inspection behavior. Both native cases, the isolated shipping build
and architecture pass (182.87 s including architecture), as do both integrated
ASan cases (1.14 s), source ratchets and ASan target coverage. This uses the
existing native-opplist worktree/build and root ASan build. Linux CI exposed a
coordinator fixture race: Dave's delayed hire broadcast could be mistaken for
Alice's next hire. A two-observer barrier fixes the reproduced 100 ms delay
negative control; all four focused native/coordinator checks pass (9.78 s).
All 15 corrected-head checks passed; the PR merged as `6244894b6ae94ba0b020aaa232bf6c644dabd5d9`. Both root shipping binaries were relinked before
the subsequent frozen-input replays. The native move-to-climb failure itself
and treatment remain unqualified.

Replay `mo9e3gae` reached real enemy fire: merc `1:2` fell from life 73 to 42,
with bleeding 31, beside Spider's 100-point kit. The attempted next move was
correctly refused because both before-states already showed enemy turn 2. The
operator helper lacked that phase check. A separate phase-aware helper now
waits through native AI and records exact interrupt passes using the existing
shared probe cursors; seven offline behavior checks pass. That stopped run
does not prove an enemy-to-player return or treatment.

Replay `vpj4fcc9` repeats native travel/entry and verified Spider equip. Her
first move to 20555 is Applied but stops at 20553. After stable ordinary control
and exact receipt verification, a distinct destination 18965 is GameplayRejected;
the original move is not replayed or claimed complete. No shot, EndTurn or AID
is attempted. Both runs close with server exit 2 (active tactical checkpoint
unsupported), clients exit 0, and unchanged source/binary/asset/private-state/
ancestry checks. The subsequent read-only trace in `qx_hi9j4` reproduces a
partial Spider walk from 17636 toward 20555, stopping at 20546. The distinct
follow-up to 18965 is rejected with native path cursor 30/30, retained final
20555, standing animation 1, delayed movement 1 and blocked tile 20547. AP 82,
breath 76 and no-AP/paused flags remain unchanged. This identifies the route
admission rejection; it does not prove the original tile-wait cause. The run
closes with the same clean immutable-input checks and tactical-checkpoint exit
limitation. The temporary trace is retained as an artifact and removed from ROOT
byte for byte; the ASan server is relinked without it.

Native carried inventory capture is [PR #894](https://github.com/tais/1vibe13/pull/894),
`82cfca2b2`. It adds exact-actor/world revision tracking, complete ordinary-object
fingerprints and typed stack totals, plus shipping linkage for the merged codec.
Unsupported graph internals remain untracked; ground context and runtime stream
activation remain separate. Native capture/codec checks pass (0.53 s), architecture
passes (199.93 s), and removing private flag hashing fails four assertions.
Its sanitizer build registration covers 162 executable targets; no additional
local sanitizer runtime pass is claimed. All 15 CI checks passed and the PR
merged as `a15095dc8708dd07a584245ea384db3ea94a77a9`. Runtime activation remains
separate from capture and does not relax active-battle checkpoint eligibility.

Reinforcement fidelity is [PR #895](https://github.com/tais/1vibe13/pull/895),
`0b7190f31`: an optional versioned 24-byte native extension captures the turn
counter, enemy/militia deadlines and backlogs. Present invalid extensions reject
before domain load; absent extensions retain the exact legacy conditional RNG
draws. Native disk-envelope/headless/runtime/eligibility tests and architecture
pass; bypassing exact restoration fails three checks. Its domain prefix is an
opaque fixture, so installed map save/load and active battle resume remain open.
All 15 CI checks passed before merge as `d32a2a8d18f4f931e80af8bcf8a5731e7d46627c`.

Server owner delivery is [PR #896](https://github.com/tais/1vibe13/pull/896),
`deb364487`: exact-owner staging waits for baseline ACK and public publication,
then publishes bounded private replacements before retained/transient receipts.
Lifecycle resets revoke tokens, and logical waiting still permits inbound ACKs.
Four focused suites including real two-client sockets pass (5.59 s), architecture
passes (206.54 s), and two independent ordering guard removals fail regressions.
All 15 CI checks passed before merge as
`0e3a04145e1c218cf9073ef15f36e8a79971ed54`. The merged reinforcement extension
and this stream do not alone activate owner inventory or battle persistence.

Passive owner inventory receive/cache is [PR #897](https://github.com/tais/1vibe13/pull/897),
`aaf5e183d`: it validates owner/session/world/baseline/actor/revision before replacing
bounded carried state; same-revision conflicts fail closed and lifecycle changes
clear the cache. Private replacements are handled before terminal receipts. An
actual socket loss during baseline/delta ACK sends becomes a resumable disconnect
without swallowing malformed-frame or allocation failures. Five focused suites
pass (6.50 s), architecture passes (311.81 s), two independent negative controls
fail as intended, and the ASan build covers 162 registered executable targets.
All 15 CI checks passed before merge as
`26585544bfefe23f362d484405802e9934c1c106`. Runtime activation, new inventory
verbs and UI remain separate.

[PR #898](https://github.com/tais/1vibe13/pull/898), `7128e64b7`, fixes a separate
native fence continuation bug: a stale cached fence cost could advance the route
and set a continuation lock despite no physical fence or pending jump. Native
preflight now checks the real fence and landing first, preserving reserved-landing
waits and existing pending jumps. The real-structure negative control fails twice
before the fix; native/headless/strict runtime, Debug ASan and architecture pass.
This is not evidence for the cause of `qx_hi9j4`'s original tile wait. The patch is
integrated into ROOT with one fixture isolation adjustment: the flat-route test
now temporarily removes the preceding traversal test's physical fence. Combined
native/headless/strict runtime/coordinator checks pass 4/4 (18.15 s), including the
merged #893 broadcast barrier. Shipping relinking has now completed with the
movement corrections below. All 15 CI checks passed before merge as
`8ae952f88e1676d7177df1d4eac01ba364e5c0e1`.

Native ordinary same-actor inventory swaps are in [PR #899](https://github.com/tais/1vibe13/pull/899),
`84ffd6247`. Placement, stack conservation, AP cost, equipment continuations and
post-mutation failure handling are native; a specialized idle-pose gate rejects
pending movement, traversal, attack and schedule work before retention and again
before execution. Six focused tests and architecture pass. All 15 CI checks
passed before merge as `6865727ffb59cb8829dbc5f158a25561c3e56c4f`;
activation through wire/server/host remains a separate dependent change.

Native schedule fidelity is [PR #900](https://github.com/tais/1vibe13/pull/900),
`7867f3427` (reviewed persistence delta `4bfbeec69`). The optional bounded SCHD section preserves the ordered schedule list
and allocator exactly, validates restored actor/event associations and replaces
reconstructed schedules without changing RNG or event state. Four tests and
architecture pass; its opaque-domain disk fixture does not prove installed
battle save/load. All 15 CI checks passed on the original head; a CMake test
registration conflict with merged #898 was then resolved by moving the identical
block. Rebuilt native schedule tests pass (0.54 s), and the new head merges cleanly
against the current base and all pending PRs. Fresh CI is pending. Tactical
checkpoint eligibility remains closed.

The reviewed passive checkpoint candidate is preserved at `e41038298` after
combining SCHD and PCVW. Its same-build shipping installed run `lmfxqcpp` passed
in 349.89 s: native creation, first client Ready, same-root exact-credential
reconnect Ready, clean strategic checkpoint, and cold-resume admission. All four
process exits were 0 and source, binary, harness, configuration and asset checks
remained unchanged. Actual client/server files contain CHKP/PGST/GRNG/RINF/SCHD/PCVW
at minute 1500 with 235 bounded profile labels. The passive client prepares this
owned projection without invoking native authority restoration. Nine integrated
suites and architecture pass. This qualifies strategic bootstrap/reconnect with
the combined codec; populated tactical schedules, ROOT static/proxy rendering,
legacy missing-PCVW refresh and active battle resume remain unqualified.
Publication waits for #900 and a fresh isolated rebase.

[PR #901](https://github.com/tais/1vibe13/pull/901), `5fa642fb4` (reviewed gameplay
delta `409d8a732`), fixes the native
long-route continuation guard. Overhead legitimately exhausts the 30-entry route,
then requests its next segment at cursor 30/30. The old guard rejected that
continuation; a real-pathfinder fixture reproduces the installed 20546/20555 stop
with delay 1 under both pathfinders. Only the safe ContinueMovement branch accepts
that exact sentinel and copies the fresh route before use; collapse/cowering and
other origins remain rejected. Three focused suites (5.30 s), strict local ASan
(1.00 s), and architecture (333.69 s) pass.

[PR #902](https://github.com/tais/1vibe13/pull/902), `317a042f0`, merged after all 15 checks
as `fc7f33921f3ddc7f90ae2279370c6fc16a503990`. It shares native move
readiness between admission and execution. It preserves ordinary steady aim poses,
rejects pending work and stale targets, checks native first-step AP cost, and
isolates admission pathfinding from campaign RNG and shared path scratch. An
explicit replacement move safely releases a completed 30/30 delayed route only
after execution revalidation. Native/host tests and architecture (415.12 s) pass;
four independent negative controls fail as intended. Both movement changes are
integrated in ROOT. Six integrated suites pass (7.95 s), and the three native
movement suites pass under ASan (4.76 s). The old combined fixture explicitly
clears its prior traversal continuation/turn lock while restoring the actor after
its independent idle-route assertions. Installed long-walk/combat qualification
is next; native tests do not establish arrival or an enemy-turn return.

[PR #903](https://github.com/tais/1vibe13/pull/903), `29e77e542` (reviewed gameplay
delta `aea30b266`), removes an unused
unconditional path-cursor read from native animation initialization. A test-only
array-bounds-instrumented translation unit reproduces the old read at cursor
30/30; real standing, cowering and recovery transitions pass after removal.
Seven focused suites and architecture (312.92 s) pass. ROOT shipping binaries
are relinked with this delta; its native fixture passes normally (0.78 s) and
with ASan plus array-bounds (1.81 s). Production remains ASan-only. The new
installed continuation `qv9bf3zg` freezes 2259 source files, 11 helpers and both
shipping binary hashes in `movement-recovery-01/fix-inputs.json` and starts from
the preserved paid medical flight. It completed five native travel legs and B13
entry, then its first kit-to-hand swap was GameplayRejected. No movement, fire,
end-turn, interrupt-pass or aid followed. Both clients exited 0; the server
returned 2 for the unsupported active-world checkpoint. All source, binary,
asset, private-state and ancestry checks remained unchanged. This is a rejected
inventory action, not passing movement or medical evidence.

A separate diagnostic continuation `74uk31bb` freezes the same sources plus two
read-only, environment-gated trace additions. They record private revision,
native orientation/animation readiness and existing ordinary-object fingerprints
without changing acceptance or gameplay state. Its trace is temporary and is not
part of a published fix. The first ordinary kit swap succeeded, with native
current/desired direction both 2 and all readiness/object proofs valid. This does
not explain the earlier rejection. Spider then reached grid 20555 from 14415 on
both replicas with Applied command 2. The first 45-second oracle timed out while
AP was 80 versus the initial 82; a later read-only observation of the same command
passed every unchanged original predicate, including exact centered stable
coordinates and AP 82. No move was replayed. Five moves ultimately passed the strict arrival oracle across both peers:
Spider 14415→20555→18965→17375 and actor 1:2 24557→20554→18964.
The next move, peer 1 command 3 to 17374, was explicitly GameplayRejected with
unchanged grid/AP. Further inputs stopped; no fire, end-turn, interrupt-pass or
aid was submitted. Both clients exited 0 and the server returned the expected
unsupported-active-checkpoint status 2; all immutable checks passed. No first-aid
or whole-campaign success is claimed. The two temporary trace sources were then
restored byte-for-byte; the production ASan server relink passed and all 2259
production source hashes match the pre-trace manifest.

The next diagnostic build integrates reviewed default-off native MOVE tracing
from `abb145d19`, with an independently reviewed three-file ROOT graft. It records
the existing admission/readiness/path-start rejection stage without altering
acceptance, receipt reasons, pathfinding calls or command ordering. ROOT's shared
decision gate and broader medical/stop behavior are preserved. Its isolated
native/host suites and architecture pass. ROOT shipping relinks pass, and the
integrated native/host tests pass normally (2/2, 0.90 s) and under ASan (2/2,
1.40 s). Continuation `y276o7ke` froze 2262 sources, 13 helpers and both binary
hashes. Five travel legs, native entry and the ordinary kit swap passed. Spider's
first move 14577→20555 passed the unchanged strict oracle after read-only AP
recovery observation of the same Applied command 2 (81→82); no replay occurred.
Command 3 toward 18965 stopped at 19760 when native combat began and exposed
enemy 270:45 at 18972. The route remained unqualified: native combat entry
explicitly stops active routes. After fresh combat-state checks, new short moves
passed for both owners: Spider 19760→19761 (AP 82→74), actor 1:2 19245→19246
(AP 84→76). The next Spider move, command 5 toward 19584, was GameplayRejected
without position/AP changes. The trace reports `admission/non-idle-pose`, native
WALKING, route 0/0, final destination equal to current grid 19761. Subsequent
read-only observations retained that state.

Source inspection identified the completed-route mismatch: Overhead clears the
route to 0/0, then the enabled `NO_STANDING_ANIM_ADJUSTMENT_IN_COMBAT` policy
retains locomotion to preserve restart costs. Move readiness's existing
`completedLocomotion` branch incorrectly requires a nonzero route size. A native
regression now drives actual Overhead arrival and both native pathfinders. The
reviewed fix `7166ae51c` admits only the exact centered, empty-route, native
retained-pose policy boundary; it preserves running restart costs and rejects
partial routes, pending work and insufficient AP. It is integrated in ROOT,
with combined diagnostic/fix publication following #902. No fire, end-turn,
interrupt-pass or aid was submitted in this run. Both clients exited 0; the
server returned unsupported-active-checkpoint status 2. All source, binary,
asset, private-state and ancestry checks passed; the operator exited 0. The run
retains seeded ancestry and does not establish first aid, full battle success,
physical input or paint qualification.

Read-only native tactical evidence is [PR #905](https://github.com/tais/1vibe13/pull/905),
`93fcd615a` (original reviewed collector `01a9a7d6c`). It records separate turn, actor, event, effect and continuation
evidence, including away actors, without ticking or changing state. It exposes
no combined checkpoint-eligible result and has no runtime caller. Seven native
suites (0.69 s), architecture (200.52 s), and sanitizer target registration pass.
Independent review found and fixed malformed item-stack dereferencing before
publication. Frame/package/transport drains and native save/load fidelity remain
separate requirements; tactical persistence is still disabled. The identical
CMake registration block was moved to avoid a pending schedule-test insertion
conflict; rebuilt native tests and architecture (239.30 s) pass.

Instantaneous runtime checkpoint evidence is [PR #906](https://github.com/tais/1vibe13/pull/906),
`90afc7bde`. A const diagnostic captures frame/tick identities and errors, message
queue/callback state, separate native/host command counts, tactical public,
private and receipt obligations, campaign delivery counts, and stored listener
lifecycle state. Missing sources, package-owned work and socket output remain
explicitly unobserved. It has no production caller, freeze or checkpoint
permission; existing drain, retirement and save/load behavior are unchanged.
Six real native/protocol suites pass (2.00 s), architecture passes (203.59 s), and
independent source review passes. Current-base, pairwise and aggregate pending-PR
merge audits are clean. These fixtures do not establish installed delivery or
tactical checkpoint restoration. Fresh CI is pending.

Runtime owner inventory activation is [PR #904](https://github.com/tais/1vibe13/pull/904),
`6fd93c4cf` (reviewed runtime delta `9f900dfca`): native private capture follows staging of the current public world
and precedes inbound processing/publication. The real two-owner transport fixture
checks privacy, baseline ACK, private-only replacement and terminal ordering.
The isolated commit was rebased onto merged #896/#897 with an identical
range-diff; shipping linkage, four focused suites (2.18 s), and architecture
(422.22 s) pass. CI remains pending. Wire activation, controller and panel are
separate dependent slices; carried summaries do not represent arbitrary item
graphs or ground inventory.

The identical CMake registration blocks for #901/#903/#904/#905 were relocated
to independent anchors; rebuilt focused tests pass, and all 21 pairs of pending
PR changes merge cleanly in a read-only local audit. Fresh CI is required for
each changed head before automatic merging.

Dependent isolated groundwork is preserved locally: inventory wire activation
`243d963e5`, controller `5df99789d`, and panel `380e530ec`. The panel uses copied
authoritative slot summaries, bounded native geometry/art, inspection/select/
swap/cancel, and no item/AP prediction. Four suites (1.09 s), architecture
(242.25 s), and independent review pass; stale choice and glyph negative controls
fail as intended. Controller slot-click-to-wire tests and separate geometry tests
do not prove queued SDL/OS input or painted UI. Publication remains dependency
ordered after #899/#904. Native first-aid extraction is now separately published.

The isolated native first-aid extraction is [PR #907](https://github.com/tais/1vibe13/pull/907),
`f796b82c6`, with a deliberately narrow idle-pose scope. Independent review reproduced false Applied results when native aid
animation mapping failed but the old idle surface remained loaded; startup now
must prove actual aid or an exact pending-aid stance transition and a valid
requested mapping. Post-callback identity checks precede all actor reads, and
partial startup failures quarantine the world before retained retries. Native
fixtures and negative controls cover those cases. On the actual merged base,
15 native/codec/inventory cases pass (2.67 s), architecture passes (205.85 s),
and independent review passes. CI is pending. Completed retained locomotion is still outside its
scope until an explicit Stop settles the actor; readiness integration is a
separate follow-up, not an installed first-aid qualification.

The reviewed movement delta and four-path native medical graft are integrated
in ROOT with raw preimage backups and exact delta checks. ROOT's 30/30 route
continuation repair and shared inventory readiness/fingerprint helpers remain
intact. Ten separately registered medical regressions are byte-identical to
#907's reviewed fixture. All 19 focused movement, host, medical and inventory
cases pass normally (3.73 s) and under ASan (6.40 s); shipping client/server
relinks pass and sanitizer registration covers 186 executable targets. The
installed client now has a bounded ordinary-controller STOP probe; it does not
normalize native state or claim queued/physical Screen input. Integrated
architecture passes (207.90 s); new installed treatment qualification is next. Artifacts and backups:
`../coop-artifacts/2026-09-28/retained-medical-root-integration/`.

Continuation `hopz3fii` (closed 2026-09-29 local time) froze 2263 sources,
15 helpers and rebuilt ASan server/normal client binaries. Five travel legs,
native entry and the queued Screen kit swap passed. Peer 0 actor 1:2's route
23432→20556 was interrupted at 21356 by native combat entry (revision 785→786,
WALKING→STANDING, exact center); its original arrival remains unqualified.
After explicit fresh-state diagnosis, new command 2 moved 21356→21355 with
strict two-replica arrival and AP 83→75. Command 3 toward 21354 was Applied
but stopped at old grid 21355, X=192870 instead of target center 190720,
Y=341760, AP still 75. Native Overhead reported a route ending one tile short.
The second arrival is unqualified and was never replayed. Source review traced
the likely gap to same-moving-pose initialization skipping first-tile geometry
when forceRestart is false; the new regression must exercise Overhead through
that next arrival, beyond the earlier dispatch-only proof. The combined MOVE
publication is held for this correction. No Stop, EndTurn, interrupt pass,
fire or aid was submitted. Both clients exited 0, server returned the expected
unsupported active-world checkpoint status 2, and every immutable input check
passed. This run retains seeded ancestry and no physical/paint qualification.

The combined correction is [PR #908](https://github.com/tais/1vibe13/pull/908),
`1fb7538e20317c8a5e010cec163e881fda2ed568`. Dedicated execution captures
accepted retained locomotion before route normalization and requests native
first-step restart while keeping the old pose for run-start cost accounting.
The actual Overhead regression now reaches two consecutive exact tile centers
for walking/running with both pathfinders, and checks one exact AP debit per
step. Disabling the new force flag fails 24 assertions; restored native/host
2/2, shipping link, architecture (287.56 s) and independent review pass.
Current-base and pending PR pair/aggregate audits pass before publication.
ROOT's exact two-path delta preserves all broader WIP; with the Stop regression,
19/19 tests pass normally (4.03 s) and under ASan (6.71 s), and integrated
architecture passes (305.78 s). Corrected installed run `jpiucr3o` is now closed
with all 2263 sources, 17 helpers, both binaries, installed assets and ancestry
unchanged. Five native travel legs, battle entry and the queued shipping inventory
kit swap passed. Two consecutive combat moves by peer 1 reached exact tile centers
(16576 -> 16735 -> 16576, AP 82 -> 71 -> 60); the second began in retained WALKING.
Ordinary controller Stop then returned both owners to STANDING without AP or
inventory changes. A native shot consumed one round and AP 63 -> 42 on both
replicas; no hit is claimed. Three native enemy-turn cycles returned stable player
control (serials 1 -> 4 -> 7 -> 10), each with one EndTurn and no interrupt pass.
These movement, Stop, shot and turn checks use controller/network inputs, not
queued Screen or physical input. The equipment swap separately uses queued Screen
input. The initial long route stopped at natural combat entry and remains
unqualified; a distinct eastward step was explicitly rejected as path-not-found.

A later move after firing exposed a separate unresolved case: peer 1 command 13,
from AIM_RIFLE_STAND toward 15455, was Applied but remained at grid 15779 off-center
(x 254869, y 251909), AP 24, animation WALKING_WEAPON_RDY (331). Both replicas agreed
across revisions 36974–38604 with no interrupt, outstanding command or phase change.
No command was replayed or issued after that failure. Enemy 270:45 reappears
on the exact stopping update (revision 36270), consistent with the deliberate
combat sighting halt in `haltForSightingUnchecked`; hidden native stop/pause/timer
fields were not observed, so the live cause remains an inference. Native sighting
halt -> ordinary Stop -> fresh movement is being tested without weakening the
partial-route guard. Evidence is
`coop-artifacts/2026-09-28/runs/ja2-coop-medical-session-jpiucr3o/` including
`raised-weapon-movement-failure.json`, the separate successful qualification files
and `session-outcome.json`. No mercenary was wounded and no AID was submitted;
treatment remains unqualified. Clients exited 0/0 and the active-battle server
exited 2 because tactical checkpoints are still unsupported. The operator exited 1
through cleanup before the manually queued close record ran; its exact signal
was not logged (`session-stop-audit.json`). This is not a
save/resume pass or complete medical/campaign pass. The seeded ancestor remains
explicit. A focused native raised-weapon reproduction now uses the existing
`native-attack-receipt-contract` worktree; ROOT remains untouched apart from this ledger.

Test-only commit `818295d48` separately proves actual native Stop transitions
retained WALKING/RUNNING to loaded STANDING for both medic and patient roles,
clears the out-of-AP marker and spends no AP/breath, including AP=0. Strict aid
then admits a sufficiently funded medic, while AP=0 still rejects medic cost.
All ten native medical cases pass (3.32 s); published #907 is unchanged. The
installed Stop helper passed independent review and four centered Stop commands
now passed in `jpiucr3o`; this does not claim mid-route recovery.

Automatic GitHub mutations were suspended when two worker turns reported
application network permission revoked. Local implementation/review continues;
GitHub publication/merge requires the ordinary authorized connection to return.
The same normal git/gh connection subsequently succeeded on read-only #900
status; the existing exact-head/15-check watcher resumed. No alternate network
route or relaxed merge gate was used.

The previous complete integrated build and **294/294 CTests passed**
(203.83 s), with all three applications and both installed drivers built.
That run covers the wider working tree, not the isolated activation branch.
The newer shipping runs below still explicitly retain their seeded ancestry;
none establishes unseeded combat, physical input or paint qualification.

The clean strict-ASan controlled victory run
`ja2-coop-controlled-victory-return-7kc0wuht` now **passes the complete path**:
native deaths and exactly one new A9 defense record at minute 1875, world
retirement, generation-8 checkpoint, automatic resynchronization of both
existing clients, shipping cold Resume and nonleader reconnect. All seven
processes exit 0; source/private state, installed inputs and binaries remain
unchanged. The four ordinary mercenaries retain their identities/contracts and
Grunty retains actor `4:5`, pending flight 2250 and transit contract end 13740.
Balance remains 13640 and the native world clock remains 112505 seconds across
cold Resume/reconnect. Final generation 13 has save SHA-256
`e13bcd7e144174c94f34cb7cb9e953c1faaf54576a4b35090cf50aa68b3af655`.
This is seeded native battle-return evidence, not unseeded combat or physical
input/paint qualification. The first shipping-only flight successor
`ja2-coop-pending-hire-flight-olwtbjuf` failed at the native first-battle
“Meanwhile…” dialog before arrival. All three processes exited cleanly and
immutable inputs remained unchanged. The shipping server lacked a handler
for this local cinematic choice; the event cannot be discarded because its
ordinary Skip path dispatches the Queen's strategic response.

The new native scene hold retains its definition, clock/world context and pause
lock. An explicit shared Skip is acknowledged first, then consumed once at a
later committed boundary before native consequences run. Time remains paused.
[PR #878](https://github.com/tais/1vibe13/pull/878) isolates the native seam
(`bcfa0c4e9`), unbound by default. Four native cases verify actual first-battle
troop creation, payment and arrival scheduling, stale contexts, subsequent
scenes and a partial native quest failure. Architecture boundaries pass
(183.98 s); strict ASan passes (part of 5/5 tests, 2.45 s). Removing either the
hold or the consequences fails the positive oracle; restored code passes.
The opt-in strict-ASan shipping successor
`ja2-coop-pending-hire-flight-viwmcyhu` now **passes** with the protocol-19
runtime/client integration: the nonleader receives the exact Applied Skip
receipt, native consequences run, and the clock stays at 112800 seconds until
the time leader explicitly resumes. Grunty arrives through the actual flight
event at minute 2250 as the same actor `4:5`, joins group `30:30`, and has native
arrival contract end 12300. Both clients retain balance 13640 and all five actors
through a generation-17 arrival checkpoint, shipping cold Resume and nonleader
reconnect. All seven processes exit 0; installed inputs, ancestor evidence and
private state, and binaries remain unchanged. The final generation-18 save has
SHA-256 `3f456bc33c526cce694625d233333e14643a5530452464762744b6195dff351d`.
This certifies the shipping scene/flight/restore path after the explicitly seeded
victory ancestor; unseeded combat, two-player combat contribution, physical input
and paint remain unqualified. The native and wire contracts are being submitted
separately from the wider runtime/client integration.
[PR #879](https://github.com/tais/1vibe13/pull/879) (`5a385c137`) isolates the
128-byte scene observation, serialized shared Skip and protocol-19 admission.
It retains checksum-validated retirement of old protocol 8/9 and experimental
14–18 reconnect records. Its complete isolated JA2 build and all 193 CTests pass
(183.18 s), including architecture boundaries; 13 integrated ASan contract/credential tests pass
(3.67 s). Removing either scene hiring exclusion or notice-lineage validation
fails the positive regression, and byte-for-byte restoration passes again.
The integrated checks now all pass: 293 tests in the complete run (65.94 s),
then the corrected architecture ratchet separately (204.21 s), with the added
hiring-exclusion case also passing. All three applications and both installed
drivers build. The shipping pass above used immutable pre-8/9-migration binaries;
those additional credential compatibility cases were verified separately.

[PR #880](https://github.com/tais/1vibe13/pull/880) (`0ff9c5f82`) extracts the
passive client campaign core from the larger integration. It retains coherent
clock, group, economy and offer observations, submits shared requests using the
current revisions, correlates exact receipts, and discards unknown outcomes on
reconnect without replay. Transport message registration, host publication and
controls remain separate from this core PR. It was initially stacked on #879 and is now rebased directly onto development
after that prerequisite merged. Its exact-head watcher requires all 15 checks.
Review found that an exact duplicate terminal receipt was rejected during
explicit session retirement. The regression reproduces the old failure for
time, travel and hiring. Retained private context now accepts only the exact
settled duplicate while all player-facing observations stay hidden; an
unrequested receipt still fails closed. Seven isolated client/transport/socket/
architecture checks pass (187.84 s), and six related integrated strict-ASan
checks pass (5.71 s). All three applications and both installed drivers rebuild,
and the complete integrated suite passes **294/294** (200.73 s).
The shipping flight evidence above predates this additional retirement fix.

Post-guard defeat `ja2-coop-controlled-defeat-return-gio7hzjy` and capture
`ja2-coop-controlled-capture-return-c42ji7l5` both pass all seven strict-ASan
processes, including native return/checkpoint, hot client synchronization,
shipping cold Resume and reconnect. The seeded-combat limitations still apply.
PRs #875, #874 and #877 merged after all 15 checks succeeded, as `47349ad7f`,
`4150679a1` and `1d05b5afd`. Battle PR #876 exposed an uninitialized native AP wound divisor in
its test fixture on Linux x86-64 and Windows; ARM did not trap. The fixture now
reads ordinary AP/BP defaults from its private VFS. Its corrected native test
passes normally (0.57 s) and under strict ASan (0.99 s); CI is rerunning at
`69008615d`. The production battle guard is unchanged by that fixture fix. All 15 CI checks
then passed, and #876 merged as `a987e256af03fc7120da8dbc38c2a7b0884d84b1`.

The traced normal run `ja2-coop-controlled-victory-return-1grd1ih4` restored
both existing clients, then correctly failed its independent history oracle:
fourteen incapacitated enemies produced fifteen identical native town-defense
records. The same duplicate history is present in the retained ASan checkpoint
`w6s8doyq`. Both ledgers were recovered from their native save sections with
source offsets and SHA-256 fingerprints in `diagnostic-native-victory-history.json`.
The qualifier now preserves the live history bytes before checking the oracle,
so rejected outcomes survive ordinary native temporary-file cleanup.

The additional ASan trace `ja2-coop-controlled-victory-return-9cri80wx`
identifies the remaining hot-load RNG consumption in
`AdjustToNextAnimationFrame`, called by native actor reconstruction. The random
idle-animation opcode can run with a saved, already-due idle counter. It now
skips choosing a new idle action while `LOADING_SAVED_GAME` is set, retaining
that counter and the exact campaign RNG state/consumption epoch. Separately,
`CheckForEndOfBattle` now owns a scoped re-entry guard: finishing incapacitated
enemies still runs their real death callbacks, but those callbacks cannot
repeat the outer victory history, morale or control effects. The new native
tests exercise actual animation interpretation and battle/death/history paths,
including ordinary live idle behavior, an unfinished battle and a later battle.
Both pass normally (0.71 s) and under strict ASan (1.92 s). The trace files were
restored byte-for-byte before the clean installed run qualified above.

Removing each guard reproduces its regression: the two-enemy native battle
writes three defense records, and saved idle reconstruction changes both its
counter and RNG consumption evidence. Restored fixes pass again in isolated
branches (battle 0.48 s, idle 0.60 s), with battle architecture boundaries
passing in 194.35 s. [PR #876](https://github.com/tais/1vibe13/pull/876)
isolates battle re-entry (corrected fixture head `69008615d`);
[PR #877](https://github.com/tais/1vibe13/pull/877) (`3e4906c44`) isolates saved idle
reconstruction and has merged. Both include the #875 CI prerequisite. All three applications,
both installed drivers and the headless dependencies build locally, and the
complete integrated suite now passes **288/288** tests (209.94 s), including
architecture boundaries. The clean strict-ASan installed run is
`ja2-coop-controlled-victory-return-7kc0wuht`; its eventual flight successor
may run only after every victory-return requirement succeeds.

The first music PR CI run passed its native music regression but failed an
existing coordinator test's immediate observation of shared-bomb reset state.
`GAMEOVER` is sent before the server clears the completed match. The dedicated
test observer now publishes the actual set size atomically and uses the
existing bounded wait for its clearing. A forced reset delay reproduces the
old failure and passes with the fix; omitting actual clearing still fails.
[PR #875](https://github.com/tais/1vibe13/pull/875) isolates this test-only change
at `56b4d0a85`. Music [PR #874](https://github.com/tais/1vibe13/pull/874) includes
that prerequisite at head `d908d3f03`; both have since merged after #875's prerequisite and all 15 checks passed.

The controlled fixture now also supports `--outcome victory`: it seeds only
enemy incapacitation after the same real Resume/time/battle-entry sequence.
Native battle completion owns deaths, history, control and world retirement.
The strict-ASan attempt `ja2-coop-controlled-victory-return-7ifgpaih` remains
**failed**. Fourteen native enemies resolved and the server committed its
post-combat checkpoint, but both clients rejected loading that checkpoint with
`PackageSaveStateError::RandomConsumed` (13). Exits were host 0, clients 2/2;
source/private state, installed inputs and binaries remained unchanged. This
does not certify victory return or pending-hire arrival. The failed checkpoint
is retained for isolated load diagnosis; no random-stream guard is relaxed.

The isolated trace identifies `PickRandomSongFromList` calling gameplay
`Random(4)` from `LoadSavedGameFromPathImpl`. Music selection now shares the
audio system's cosmetic RNG with ambient scheduling. The new native regression
discovers two-track playlists and verifies that repeated victory/quiet mode
selection preserves the exact campaign checkpoint and consumption epoch even
without an audio device. It passes normally (0.59 s), in isolation (0.49 s) and
under strict ASan (1.00 s); restoring the original gameplay-RNG call makes the
test fail. Architecture boundaries pass (187.71 s). Temporary trace edits were
removed, restoring both instrumented files byte-for-byte before the fix build.
The retained failed checkpoint now loads in a fresh shipping host and both
clients (`ja2-victory-load-fixed-_p1jsk3i`): all three exit 0 and the source state
is unchanged. [PR #874](https://github.com/tais/1vibe13/pull/874) isolates this
music fix initially at `cbae5f845`; its updated head and complete combined
victory-return evidence are recorded above.

The separate `full_engine_coop_installed_pending_hire_arrival.py` qualifier is
prepared to run an already-paid flight on shipping hosts after a qualified
controlled victory. It permits observations and scoped leader time controls, with a separate
explicit opt-in for a first-battle scene Skip by the nonleader. It requires exact
native arrival/contract/group/funds behavior, then saves, resumes and reconnects.
The shipping pass is recorded above; the ancestor's seeded-combat limitation
remains explicit.

[PR #873](https://github.com/tais/1vibe13/pull/873) isolates the actual native
paid-hiring transaction from runtime activation. It revalidates economy/quotes,
constructs the pending actor/event, writes exact salary/deposit/history rows,
and identifies partial failures that require fail-stop. Five isolated native
tests pass (2.29 s), architecture boundaries pass (182.23 s), and the transaction
test passes under integrated ASan (1.42 s). All 15 CI checks succeeded on exact
head `e3bf10f37`, and the authorized watcher merged the PR at
`b1d5a29aa5fb660dd12979c754a6dcfa9ac14c1d`.

The strict-ASan music-fix rerun `ja2-coop-controlled-victory-return-w6s8doyq`
remains **failed**: native victory/unload/checkpoint succeeds, but both existing
clients still encounter `RandomConsumed` during automatic resynchronization.
Fresh-process restore of the retained prior checkpoint passes as recorded above;
the subsequent `9cri80wx` trace identifies that second draw in idle-animation
reconstruction, as recorded above.
Host/client exits are 0/2/2 and all immutable-input checks pass. The flight
qualifier did not launch because its complete victory-return prerequisite failed.
All three applications and both drivers build with the music fix. The integrated
suite recorded 285 passing tests before interruption of its final architecture
check for the additional load diagnosis; this is not a completed 286-test pass.

The guarded background watcher merged both remaining September 27 PRs after
all 15 checks succeeded on their exact heads: prisoner Resume
[#868](https://github.com/tais/1vibe13/pull/868) at `a2f4f0ef0`, and native
knowledge expiry [#871](https://github.com/tais/1vibe13/pull/871) at `0abf5a75d`.

The controlled scenario keeps ordinary server Resume, leader time control
and nonleader A9 battle entry. Only then may the test-only entry host consume
one trigger scoped to the observed native world/turn. It seeds defender health
and lets native death cleanup and `CheckForEndOfBattle` produce the loss,
capture, history, sector control and held notice. The client must explicitly
acknowledge; the normal runtime must retire the world, checkpoint, resynchronize
both clients and support a fresh shipping-host Resume plus reconnect. This
runner now passes both defeat and capture under strict ASan, including shipping
cold Resume and nonleader reconnect. It does not qualify unseeded combat.

The first controlled defeat-return attempt `ja2-coop-controlled-defeat-return-5dpvaonz`
remains **failed**. Native loss history, the held notice, exact nonleader Applied
acknowledgement, native unload and the server checkpoint marker were observed;
the runner rejected the resulting roster before qualifying that checkpoint.
Its oracle had incorrectly copied pre-entry `inSector=false` into the expected
post-battle roster. Native `TrashWorld` deliberately retains player actor flags,
and save/load serializes them. `LoadSavedGame` then calls `UpdateMercsInSector`,
which clears them with no loaded sector. The validators now require historical
flags after live battle return and cleared flags after cold Resume, while
retaining false for the pending hire and transferred prisoners. World retirement is separately
checked through campaign phase, cleared client tactical controls and save headers.
All three normal executables and both installed drivers build, and all 284
integrated CTests pass (223.94 s). The updated native economy regression also
checks the retained flag without a loaded world.

The next defeat and capture attempts, `ja2-coop-controlled-defeat-return-ujiwjoay`
and `ja2-coop-controlled-capture-return-i2656nfv`, remain **failed**. Both passed
explicit acknowledgement, world retirement, new checkpoints and two-client
resynchronization, then resumed successfully on shipping hosts. Their cold
validator rejected the native flag clearing described above; every other
semantic value matched exactly. All six processes per run exited 0 and immutable
inputs matched. Corrected validators require that precise transition without
ignoring flags during same-host reconnect. Successful complete runs are recorded below.

The strict-ASan controlled defeat run `ja2-coop-controlled-defeat-return-ybn0q69t`
now **passes the complete path**. Starting at the paid-hire generation 4, it
enters native A9 battle, holds the real defeat/sector-loss notice for 12.05 s
(53 paired samples), observes the nonleader's exact Applied acknowledgement,
retires the world and commits generation 8. Both clients resynchronize with
cleared tactical controls. A fresh shipping host restores four native deaths
and Grunty's unchanged pending flight; a new nonleader process reconnects with
the same live identities and designated leader. Final shutdown commits
generation 13, at minute 1875 with balance 13640. All seven processes exit 0,
and source/private state, installed inputs and binaries remain unchanged.
Final save SHA-256:
`b2fa05d27b19cd95e0d10e00d9cac90dbcb4c59215e61fd1a25c1d491e8a4f27`.
The qualification scope is `seeded_native_defeat_battle_return`, with unseeded
combat, two-player combat contribution, victory, hire arrival, gear recovery,
physical input and painting explicitly unqualified.

The capture retry `ja2-coop-controlled-capture-return-725tmk23` remains **failed**:
its held-notice oracle treated a native group revision update (3 to 4) as a
replacement notice, although notice 2, the paused clock and world 1 were stable.
No acknowledgement was sent. Its shutdown correctly rejected a checkpoint with
the tactical world still active (host 2, clients 0); immutable inputs matched.
The validator now retains the exact notice identity/outcome/sector and requires
nondecreasing observation revisions, a paused unchanged clock, and the original
loaded world. A new regression covers that update and rejects revision regression,
notice replacement, time advancement, unpause and world retirement. All nine
controlled-runner tests pass. The full 284-test suite passes after the cold-flag
correction (211.63 s), and all four affected runner CTests pass after the revision
correction (0.47 s). The complete capture retry is recorded below.

The strict-ASan controlled capture run `ja2-coop-controlled-capture-return-cmr8ab2e`
also **passes the complete path**. Native loss moves Gary and Bull to the first
Alma prison while Kaboom and Vinny complete death cleanup. Grunty remains in
transit with arrival minute 2250 and contract end 13740. The shared capture and
sector-loss notice remains held for 12.07 s (53 paired samples), then the
nonleader's explicit acknowledgement receives its exact Applied receipt.
Native unload commits generation 8 and clears both clients' tactical controls.
A fresh shipping host restores the exact cohort, a new nonleader process
reconnects without changing live identities or leadership, and shutdown commits
generation 13. Minute 1875 and balance 13640 remain unchanged. All seven processes
exit 0; source/private state, installed inputs and binaries remain unchanged.
Final save SHA-256:
`7caab06c5ea61d1f53d0937510c032fb1d3d80c7b7a04c7ecde4d927e3d036f5`.
Its scope is `seeded_native_capture_battle_return`, with the same explicit
unqualified areas as the controlled defeat run. The independently verified
artifact index is `coop-artifacts/2026-09-28/controlled-battle-return-result-index.json`
in the parent project directory.

Repeat the controlled battle-boundary scenarios from the qualified paid-hire
save after building the installed entry/client drivers and shipping executable:

```sh
python3 tests/full_engine_coop_installed_controlled_battle_return.py --source-evidence /path/to/qualified/hiring-evidence.json --outcome defeat
python3 tests/full_engine_coop_installed_controlled_battle_return.py --source-evidence /path/to/qualified/hiring-evidence.json --outcome capture
```

[PR #872](https://github.com/tais/1vibe13/pull/872) isolates the actual native
friendly-group, employment/funds and AIM-offer readers from runtime activation.
Its native group/economy tests pass in the clean worktree (2/2, 1.02 s), as does
the architecture check (227.62 s). Both readers pass under strict integrated
ASan (2/2, 2.78 s), including malformed groups, stale handles, all 260 native
player slots, pending hires/vehicles, ledger mismatches and invalid prices.
All 15 CI checks passed on exact head `da9ea567e`, including Linux ASan and all
three applications on Linux, Linux ARM64, macOS and Windows. The authorized
watcher merged the PR at `2ecc858310192206b4fbfed044370675726f409d`.

PRs [#862](https://github.com/tais/1vibe13/pull/862) and
[#863](https://github.com/tais/1vibe13/pull/863) merged after all 15 checks
succeeded on each exact head. The native battle notice and shared contract
remain separate from the integrated runtime activation.

The actual-defeat run `ja2-coop-defensive-entry-ydttglmk` remains **failed**.
All four defenders died and both clients observed the same native defeat and
A9 town-control loss. The nonleader submitted Continue and Enter, but no exact
Applied receipt was observed before the server rejected its required checkpoint
because native dialogue was still active. No post-battle checkpoint is certified.
[PR #864](https://github.com/tais/1vibe13/pull/864) adds a bounded native exit wait
with admission closed and the full checkpoint eligibility check retained.
It merged at `dc5a73de2` after all 15 checks passed on exact head `44b79b447`.

[PR #865](https://github.com/tais/1vibe13/pull/865) isolates strict cold-resume
classification for traveling/wounded squads, exact paid pending hires and
completed native deaths. This includes the last living merc dying while a paid
hire remains en route. Three native/policy tests pass in isolation (1.02 s),
also under integrated ASan (1.36 s); isolated architecture checks pass (185.94 s).
The Linux ASan job exposed missing difficulty data in the real death fixture;
its corrected setup initializes the native progress divisor and SAM-sector
data. The corrected fixture passes isolated (0.53 s) and integrated ASan
(0.96 s). The corrected Linux ASan job passes. After resolving the independent
sector-label test registration overlap, the three native tests pass (1.54 s)
and all 15 CI checks passed on exact head `bbc7b5e1b`. It merged at
`9f41c6a0e`. An unseeded post-battle loss restart remains unqualified.

The subsequent run `ja2-coop-defensive-entry-fsc5t1cm` also remains **failed**.
Both players moved and fired; the nonleader declined surrender. Native gameplay
later captured the surviving defenders and published capture plus town loss,
but the server crashed in the tactical overlay frame before acknowledgement
(server -11, both clients 0). Source/private state, installed data and binaries
remained unchanged. A native regression reproduces a 50-character dialogue
sector-label buffer being overwritten by the formatter's assumed 60-character
capacity. [PR #866](https://github.com/tais/1vibe13/pull/866) now uses actual
caller capacity; the native regression passes normally (0.62 s), under strict
ASan (1.01 s), and in isolation (0.54 s). Isolated architecture checks pass
(186.58 s). It merged at `ea992efae` after all 15 checks passed on exact head
`31a73ceba`.

The diagnostic ASan run `ja2-coop-defensive-entry-zlogl1zx` stopped earlier,
during cold Resume: ambience restoration indexed the absent tactical tileset
(ID -1). It reached neither admission nor combat. That independent load error
is fixed in [PR #867](https://github.com/tais/1vibe13/pull/867), which skips
tactical ambience without a loaded world and checks any loaded tileset ID.
It merged at `fdedaaf6e` after all 15 checks passed on exact head `966446a44`.
The retry `ja2-coop-defensive-entry-59cgry3i` passed cold Resume and reached a
native defensive encounter, then ASan stopped it on a trait-label table read.
That run is **failed**, with no acknowledged outcome or cold checkpoint.

The installed travel seed had changed to old-style traits after native
profiles and actors had loaded new-style trait IDs. It now preserves the loaded
trait system and checks all seeded actor/profile trait IDs before checkpointing.
The old September 26 seed and descendants are retained as diagnostic evidence;
they must not qualify a completed campaign. The fresh ASan run
`ja2-coop-campaign-actions-w1pk7jyq` passes nonleader travel and explicit retreat,
leader-only time advancement, a matching peaceful A9 return on both clients,
and clean checkpoint generation 3 at minute 1870. Its save SHA-256 is
`0312891472911567f9c8315f7ddefc8336808f89759d81e88f21ae368cc83700`.
The shipping ASan hiring run `ja2-coop-hiring-2ysc22la` also passes: the
nonleader confirms Grunty `4:5`, both clients agree on one salary/deposit debit,
and his pending arrival at minute 2250 is saved in checkpoint generation 4
(SHA-256 `66da364a898d8924886b10da69c7aa602029d6a425aad441e9193ce3a8caba34`).
The fresh ASan battle-return run `ja2-coop-defensive-entry-ygo3sdns` failed:
both players moved and fired, but both clients aborted on an out-of-bounds
sprite depth-table read during animation. A finite-profile regression reproduces
the read under ASan; [PR #869](https://github.com/tais/1vibe13/pull/869) bounds
opaque, transparent and left-clipped lookups and passes all seven blitter variants.
The isolated renderer test passes (0.82 s) and architecture boundaries pass
(200.95 s). Native death, native capture and renderer
tests pass together under strict ASan (2.60 s).
It merged at `bf675a47c` after all 15 checks passed on exact head `4a439e4df`.

The retry `ja2-coop-defensive-entry-hhm4b87a` also failed, after both players
moved and fired. The server aborted (-6) when an enemy bullet replaced a window
structure and then dereferenced that freed structure in the following shield
check (`MoveBullet`, `LOS.cpp:7824`). Both clients exited cleanly and all source,
private-state, installed-data and binary fingerprints remained unchanged.
[PR #870](https://github.com/tais/1vibe13/pull/870) skips the stale-pointer path
after the native window swap. Its native fixture reproduces the exact ASan
use-after-free before the fix, then passes all eight orientation/direction cases
under strict ASan (1.01 s) and in isolation (0.49 s); isolated architecture
boundaries pass (184.48 s). It merged at `5d8cfb5ce` after all 15 checks passed
on exact head `d0c6ddd79`.

The retry `ja2-coop-defensive-entry-670giwri` remains **failed**. Both players
moved and fired; ordinary native turns reached serial 23 before ASan detected
a public-opponent knowledge table overread in `UpdatePublic` during expiry.
The server exited -6, both clients exited 0 and all immutable inputs matched.
[PR #871](https://github.com/tais/1vibe13/pull/871) checks the explicit forget
request before indexing the table with transient expired knowledge. Actual
native hearing and sight decay both reproduce the ASan error before the fix
and pass afterward (2/2, 1.11 s). Both isolated cases and architecture checks
pass (3/3, 181.03 s). All 15 checks passed on exact head `72c0695b4`; it merged
at `0abf5a75d`.

[PR #868](https://github.com/tais/1vibe13/pull/868) adds
read-only cold-resume validation for the first native Alma prisoner cohort.
It checks the real capture count, quests, unique insertion positions, completed
death cleanup, empty inventories and absence of squad/group membership while
preserving the exact paid pending hire. The fixture uses actual native capture;
the old classifier fails six positive cases. Four isolated native/policy tests
pass (1.17 s), three ASan cases pass (1.02 s), and isolated architecture checks
pass (190.80 s). It now targets the main development branch after #865 merged;
six native/policy cases pass after updating through #870 (2.27 s). All 15 checks
passed on exact head `346fb614e`; it merged at `a2f4f0ef0`. Controlled installed prisoner persistence passes
as described below. Acknowledged capture from actual combat remains unqualified.

At the user's request, repeated full-battle crash hunting no longer gates all
campaign persistence work. The alternative runner
`tests/full_engine_coop_installed_cohort_restore.py` starts from the qualified
paid-hire checkpoint and uses the test-only entry host's native death cleanup
or first-capture routines to create a controlled cohort. The normal runtime
must commit it; a fresh shipping server must restore it for two read-only
clients, preserve exact profiles/contracts/funds/clock/pending hire across
nonleader reconnect and commit another checkpoint. No tactical battle outcome
or acknowledgement is fabricated. Evidence explicitly excludes combat,
battle return, confiscated-gear recovery, hire arrival and physical input.
Ten runner checks pass, including corrupt cohorts, stale/mismatched identities,
contract/clock/fund drift, reconnect mutations and seed checkpoint rotation.
The strict-ASan controlled death run `ja2-coop-cohort-death-lbg90e8e` passes:
the native fixture committed generation 5, a fresh shipping host restored all
four completed deaths plus the exact pending Grunty hire, both clients agreed,
the nonleader disconnected/reconnected without changing identities, and normal
shutdown committed generation 9. Both paused holds retained minute 1870 and
balance 13640. All five processes exited 0; source/private state, installed data
and binaries stayed unchanged. Final save SHA-256 is
`85f2819d691a51c7d14f358e07b9cc97cdcc5572e20ea759d875a66b0512261e`.
Its evidence scope is `seeded_native_death_cohort_restore`; it does **not**
certify a combat outcome, loss acknowledgement or tactical unload.
The strict-ASan controlled capture run `ja2-coop-cohort-capture-oegxieho` also
passes the entire path: generation 5 contains two completed deaths, two native
Alma prisoners and the unchanged Grunty flight. A fresh shipping host restores
those exact profiles/assignments/contracts, both clients agree, the nonleader
reconnects with unchanged identities, and shutdown commits generation 9. The
clock remains at minute 1870 and balance at 13640. All five processes exit 0;
source/private state, installed inputs and binaries remain unchanged. Final
save SHA-256 is
`75b4a119f73f95dfad6ce925a76f94bc35a64fa75c75dc10309add7faa8451c5`.
Its evidence scope is `seeded_native_capture_cohort_restore`, with the same
explicit combat/acknowledgement/unload exclusions as the death scenario.

Repeat either controlled case from an immutable, successful pending-hire
`hiring-evidence.json` using the explicitly supplied source path:

```sh
cmake --build build --parallel 6 --target JA2_ENGLISH full_engine_coop_installed_entry full_engine_coop_installed_client
python3 tests/full_engine_coop_installed_cohort_restore.py --source-evidence /path/to/qualified/hiring-evidence.json --outcome death
python3 tests/full_engine_coop_installed_cohort_restore.py --source-evidence /path/to/qualified/hiring-evidence.json --outcome capture
```

For an ASan build, explicitly point `--fixture-executable`, `--server-executable`
and `--client-executable` at that build's binaries. Each run owns separate
private state; the source evidence/save and installed inputs must remain
unchanged. The seed checkpoint is preserved outside the rotating runtime slots.
Only native cohort persistence can pass here; the existing battle-return and
post-battle restore runners retain their stricter outcome/acknowledgement gates.

PRs [#857](https://github.com/tais/1vibe13/pull/857),
[#859](https://github.com/tais/1vibe13/pull/859),
[#860](https://github.com/tais/1vibe13/pull/860) and
[#861](https://github.com/tais/1vibe13/pull/861) also merged on September 27,
each after all 15 checks succeeded on the exact reviewed head. They cover
shared surrender requests, native interrupted-attack receipts, suppression
stance costs and authoritative stance affordability.

The integrated checkout now uses protocol **18** (snapshot 12, delta 11, SDK
0.3). Native defeat/capture callbacks retain their real outcome and pause for
an explicit shared acknowledgement. Either ready player may select Continue
and confirm Enter. Acknowledgement does not unload inline: the runtime queues
its receipt, drains tactical work and closes ingress before a later native
unload and the existing strategic checkpoint barrier. New native notices are
merged in [PR #862](https://github.com/tais/1vibe13/pull/862); their
portable shared contract is merged in [PR #863](https://github.com/tais/1vibe13/pull/863).
The executable activation remains in the integrated checkout.

The explicit-loss run `ja2-coop-defensive-entry-es9v8hht` remains **failed**.
Both peers fired once, then ended ordinary turns; the native game captured the
two remaining unconscious mercs after two observed deaths. The server published
kind 3 (capture), then stopped on the separate A9/Omerta sector-loss message.
All source/private-state, installed-data and binary fingerprints remained
unchanged; server exit was 2 and both clients exited 0. No acknowledgement or
post-battle checkpoint succeeded in that run.

The follow-up adds the committed contested-town control-loss flag to the exact
shared outcome, retires its earlier notice ID, and guards native unload calls
until explicit completion. It retains unrelated sector, mine and SAM notices.
Six native notice cases and status/action/time/client tests pass (10/10, 1.01 s),
also under strict ASan (10/10, 2.36 s). The separate `--finish-capture` qualifier
requires kind 3 plus visible sector loss on both clients, actual prisoners at
the configured initial Alma prison, their exact death/prisoner partition,
unchanged contracts/funds/pending flight, explicit nonleader acknowledgement,
cold checkpoint and synchronized return. It never qualifies a victory or an
all-dead defeat. Installed capture return and cold Resume remain unqualified.

All **283 CTests** pass after the prisoner-resume, sprite-depth, native
window-collision, knowledge-expiry and controlled-cohort changes (206.26 s,
`controlled-cohort-integrated-tests.log`). All three shipping applications and
both installed drivers build. The controlled installed scenarios use the same
ASan shipping server and corrected trait-preserving source; full battle return
and restart from an acknowledged combat outcome remain separate acceptance work.

The protocol-18 run `ja2-coop-defensive-entry-z_0ck411` remains **failed**:
both players moved, contributed 23 confirmed shots (9 and 14), changed stance
with native AP costs, reloaded and explicitly declined surrender. It reached
the 16-player-turn bound with no native outcome, so it proves neither victory
nor defeat return. The server exited 2 and both clients 0; source state, assets
and binaries stayed unchanged. The separate explicit loss scenario now fires
one confirmed shot per peer and then uses ordinary EndTurn. It still requires
a new native loss record, the exact shared notice and acknowledgement receipt,
post-battle checkpointing and reconnect; it never counts a loss as a victory.
Its new strategy, notice and cold-restore checks pass 55 Python tests (0.100 seconds).
Installed acknowledgement and cold restore are **not yet qualified**.

On September 27, PRs [#855](https://github.com/tais/1vibe13/pull/855),
[#856](https://github.com/tais/1vibe13/pull/856) and
[#858](https://github.com/tais/1vibe13/pull/858) merged after all 15 checks passed
on each exact head. The installed qualifier now permits a connected player to
continue observing after losing every controllable defender, retains earlier
contributions, and still rejects missing participants and replica disagreement.
Its observer and stance checks pass all 62 Python tests (0.172 seconds).

The September 27 run `ja2-coop-defensive-entry-hq6a8cyc` remains **failed**.
Both players moved and six shots completed. Actor `3:4` then stood up on turn 4:
both replicas progressed from crouched aim through `KNEEL_UP` to `STANDING`,
with an Applied receipt, but AP stayed at 50. The qualifier correctly rejects
that free combat stance change. Source state, installed assets and binaries
remain unchanged; server exit is 2 and both client exits are 0. No victory,
observer continuation, surrender response or hire arrival is certified by this run.

A native regression reproduces the suppression waiver leak for both crouch and
prone: suppression already charges its intended 20 AP, then leaves a waiver
that discounts the next player stance change. Removing that stale assignment
preserves the suppression penalty and native completion counter while restoring
the later action's full AP cost. All four native modes pass normally (0.86
seconds) and under strict ASan (1.44 seconds). All three shipping applications
and both installed drivers rebuild. Logs and new run artifacts are under
`/Users/mathijs/Projects/bughunt/coop-artifacts/2026-09-27`.
The fix is isolated in [PR #860](https://github.com/tais/1vibe13/pull/860).

The fresh run `ja2-coop-defensive-entry-zywfsjjp` verifies the fix in actual
replicas: four stand-up actions complete with native AP expenditure, including
actor `3:4` at 50 → 44. Both players contribute 17 shots (9 and 8), both move,
one normal reload conserves eight rounds from the reserve, one player interrupt
pass completes, and the nonleader explicitly declines surrender. Ten player
turns are observed. Peer B loses its last controllable defender at revision
6402 and peer A at 6404; both remain admitted observers and keep receiving
matching state. The native defeat timer then opens “You have been defeated in
this sector!”, which the dedicated process does not yet handle. The run remains
**failed**, with no post-battle checkpoint, cold restore, victory or hire arrival.
Server exit is 2, both client exits are 0, and source state, installed assets and
binaries are unchanged. Failure history is retained with SHA-256
`a58444229a285ecf48c6689e6d9ad54e2dfd87a248c8a301793acdf8b9d79643`.
Defeat/capture notice continuation is now a confirmed native integration gap.

A separate server-admission regression shows that remote stance commands
bypass the normal UI affordability check: five AP could buy a six-AP crouch,
and a movement turn exemption could make a later explicit prone rise free.
The dedicated executor now checks the complete native stance cost before any
transition and resets the movement-turn exemption only for an accepted player
request. It also rejects noninterruptible native animations; forced system
stances retain their original policy. All five native modes pass normally
(0.96 seconds).

The interrupted-attack portable receipt contract is isolated in
[merged PR #859](https://github.com/tais/1vibe13/pull/859).
Three protocol/client/server tests pass normally (0.58 seconds) and under ASan
(0.65 seconds), together with architecture and ASan coverage checks. The new
reason is legal only for terminal cancellation; this foundation does not yet
activate the integrated protocol-17 native completion and client feedback.

The September 27 integrated rebuild now passes all **269 CTests** (386.37
seconds), including both new native stance regressions and the merged transport
fixture improvement. All five focused native modes also pass strict ASan (1.57
seconds). Logs are `stance-validation-all-build.log`,
`stance-validation-all-tests.log` and `stance-command-asan-tests.log` in the
September 27 artifact directory. The server-admission fix is isolated in
[merged PR #861](https://github.com/tais/1vibe13/pull/861).

The earlier September 26 rebuild passes all **267 CTests** (200.16 seconds),
including architecture boundaries and the protocol-17 attack lifecycle. Eight
focused native, protocol, client, loopback-server and credential tests also pass
under strict ASan (6.25 seconds); the final native ownership, stale-incarnation
and existing-action controls pass in all three native modes (1.30 seconds).
All three shipping applications and both installed-test executables build.
Logs are `attack-ready-ctest.log`, `attack-v17-asan-tests.log`,
`attack-ready-asan-tests.log`, `attack-ready-all-build.log` and
`attack-ready-drivers-build.log`. Evidence from this
date is stored under `/Users/mathijs/Projects/bughunt/coop-artifacts/2026-09-26`.
The earlier temporary run directories were cleared during the pause; the
September 22 results below are historical records, not currently available
save artifacts. New installed runs regenerate their source checkpoints.

The fresh `ja2-coop-campaign-actions-u5qbzk2a` run passes nonleader travel,
confirmed native retreat, leader-controlled time and the peaceful A9 return.
Its clean generation 3 slot A checkpoint is at minute 1870 with four mercenaries,
SHA-256 `5d7517a2c9a41b0839f46ebf35d1f35157cece1f1b7a50251fe3201526176a79`.
This is the fresh source for pending hiring and the defensive-battle rerun.

The fresh `ja2-coop-hiring-lwhqd5kv` run also passes. The nonleader hires Grunty
(profile 6) for seven days without equipment for exactly $5,500, reducing shared
funds from $19,140 to $13,640. Both clients and the server exit cleanly. Generation
4 slot B retains actor `4:5` pending at minute 1870 with arrival scheduled for
minute 2250, SHA-256
`0cc0983604d6a778df7f463861e8df25da479106b75e73ac8cd372d571b3a217`.
Both offer screenshots were reviewed; that review does not certify complete
tactical painting or physical keyboard input.

The next full-battle run, `ja2-coop-defensive-entry-ya2f2v27`, remains **failed**.
Both players moved and completed five native shots, including the previously
stalled off-turn hit-recovery case. During the next enemy turn the dedicated
host logged an unhandled native `Surrender?` dialog and screen-handler failure.
Three defenders were still alive; no victory was witnessed. The owned server
was explicitly stopped after diagnosing that hold. Its teardown removed the
private live history before the qualifier's next read, so the final JSON's
failure is the secondary missing-history error; `operator-stop.json` and the
copied `unsupported-surrender-history.dat` preserve the original diagnosis.
Server exit was 2, both client exits were 0, and source saves, installed assets
and binaries remained unchanged. This run proves neither victory nor hire arrival.

The surrender rerun `ja2-coop-defensive-entry-yqq47eom` also remains **failed**
as a complete battle. Its nonleader explicitly confirms offer 1 with `N`, then
released `Enter`; both clients observe the exact Applied campaign receipt and
clear the offer. Native combat resumes. Five shots and both movement
contributions are confirmed. A later FIRE command 6 by actor `0:1` receives
Applied, then native sighting cancels it while leaving cower. No round is spent.
The qualifier preserves that final command as unattributed and reaches its
600-second deadline (the final error is the campaign probe at that deadline).
Its failure history is copied before teardown. No victory checkpoint exists.

The next implementation uses protocol **17** and retains each remote attack's
native shooter through completion. Dispatch alone no longer produces a terminal
Applied receipt. Actual native cancellation without a discharge gets a distinct
Cancelled reason, allowing the client to explain the outcome and a later fresh
intent to proceed. The real sighting callback and attack-busy drain tests pass
normally and under ASan. The rebuilt installed rerun is retained below; battle
completion remains unqualified. The native seam is
[merged PR #856](https://github.com/tais/1vibe13/pull/856);
that PR remains unbound by default and excludes the integrated protocol/UI.

The protocol-17 run `ja2-coop-defensive-entry-knhqg5oq` remains **failed** as a
complete battle. Both players move and complete four shots in total; a native
player interrupt pass and explicit nonleader surrender decline also complete.
At revision 4592 the qualifier rejects the shared ownership set. Both replicas
actually agree: actor `1:2` is dead, actor `2:3` is unconscious at health 10,
and the two conscious survivors `0:1` and `3:4` each have one owner. The native
`OK_CONTROLLABLE_MERC` rule correctly removes control below `OKLIFE` (15); the
qualifier incorrectly required ownership for every living actor. Its corrected
check records unconsciousness separately from death, retains exact identities
and native poses, permits recovery at 15, and still rejects missing healthy
ownership, retained unconscious control and equal-revision contradictions.
Both defensive Python suites pass all 56 tests (`unconscious-defender-tests.log`),
and the corrected check accepts the retained final actor observations without
changing the failed run's evidence. The server exits 2, both clients exit 0,
and source saves, installed assets and binaries remain unchanged. Native history
is retained before teardown; there is no victory or hire-arrival witness.

The corrected qualifier's fresh run `ja2-coop-defensive-entry-bkpjj150` also
remains **failed** as a complete battle. Both players move, five shots complete,
and the nonleader explicitly declines surrender. It passes the earlier
unconsciousness transition and continues through 14 observed player turns.
At revision 7569 / turn 47 both clients agree that `1:2` and `2:3` are dead,
`3:4` is unconscious at health 14, and only `0:1` is controllable (health 60).
The current qualifier stops because one peer has no controllable defender;
this is its two-owner scope limit, not an observed replica disagreement or
native victory. Continuing battle qualification must distinguish an admitted
peer observing without an assigned actor from a lost participant, while
retaining the already proven contributions. Source state, assets and binaries
remain unchanged, server exit is 2 and client exits are 0. Failure history is
retained with SHA-256
`08b4ebca7962c3dfd2f088b8d7b9b9467c37b0e3127854732944c13210f8986f`.
There is still no victory checkpoint, victory restore or completed hire arrival.

Protocol **16** now represents the exact pending native surrender offer in the
campaign status and adds explicit decline/accept actions to the shared request
barrier. Either ready player may answer once. Client confirmation requires a
choice followed by a separately pressed Enter. Native gameplay and checkpointing
hold while transport and observations continue. The native continuation retains
world, turn and speaker incarnation and preserves native quote RNG consumption.
Its focused tests pass normally and under strict ASan, including duplicates and
stale contexts. The new installed rerun explicitly requests `--surrender-choice
continue-fighting`; a qualifier without that option cannot silently answer.
Acceptance invokes the existing native capture path, but captivity and subsequent
persistence still require qualification.

[PR #849](https://github.com/tais/1vibe13/pull/849) and
[PR #853](https://github.com/tais/1vibe13/pull/853) are also merged after all 15
checks passed on each exact head. The passive actor directory is also merged in
[PR #854](https://github.com/tais/1vibe13/pull/854) after all 15 checks passed,
with isolated normal and ASan tests covering all 2,048 slots and 4,096 replacement
operations. Native surrender continuation is in
[merged PR #855](https://github.com/tais/1vibe13/pull/855); that isolated seam is
unbound by default and does not enable the integrated runtime or protocol-16 UI.
The portable offer/status and shared decline/accept serialization is isolated in
[merged PR #857](https://github.com/tais/1vibe13/pull/857). Its four campaign tests
pass normally (0.07 seconds) and under ASan (0.13 seconds) on updated head
`0534b047c3bceefa19e7b0c134ad2211c8acf393`, including reserved
bytes, retired offer IDs, competing answers and duplicate outcomes. That PR
retains the development branch's protocol version and requires a later explicit
activation alongside the native hold and client confirmation.

PR #855's first Windows test run passes its native surrender tests but fails
the existing `mp_sdl_net_transport` test while binding its required IPv4
reserved-loopback listener. The test tried 128 adjacent ports; its old diagnostic
does not report the underlying bind error. The failure is retained in
`pr-855-windows-failure.log`. That single failed job passed on its unchanged-head
rerun, and #855 merged after all 15 checks succeeded.
The bounded port-selection and bind-diagnostic improvement is isolated in
[merged PR #858](https://github.com/tais/1vibe13/pull/858), without a production
transport change. Five consecutive real transport runs pass (26.26 seconds),
as does strict ASan (5.81 seconds); architecture and ASan coverage checks pass.

The user-confirmed campaign policy permits any campaign-ready player to order
shared travel, choose supported battle actions, hire mercenaries and spend
shared funds. Only campaign time advancement and speed belong to the designated
time leader. Authoritative hiring and its client interface are now being
implemented and qualified under that policy.

Protocol **14** introduced the bounded `coop.campaign.action.request` (56 bytes) and
`coop.campaign.action.result` (80 bytes) API. The shipping client and dedicated
runtime now connect adjacent surface/on-foot travel, wilderness-NPC
acknowledge/stop, prepared battle entry with native forced insertion or explicit
Spread, and supported native retreat. Authentication, checkpoint readiness,
observed control/group revisions, exact group/decision identity and fresh native
validation all precede execution. Shared actions and leader-only time requests
consume the same control revision before calling native gameplay. Retained
terminal outcomes survive backpressure and same-transport resync; duplicates
cannot repeat a native action, and reconnect does not replay an unknown outcome.
The server queues current status/group observations before a terminal action
receipt can release client input.

Protocol **15** added complete economy
and AIM-offer observations and a separate 56-byte hire request / 96-byte receipt.
The complete friendly roster includes mercenaries still in transit and up to
254 mercenaries plus six vehicles. The server rechecks current availability,
willingness, prices, shared funds and ledger append readiness before creating
one native actor and delayed arrival. It then records salary, any enabled
medical deposit and hiring history through checked native writers. Any failure
after mutation requires fail-stop; cached authenticated receipts suppress
duplicate execution. Quotes are queued after the exact economy they reference,
and receipts wait for current status/groups/economy/quotes to be queued. An
Applied client additionally verifies the exact hired actor and profile.

The actual native paid-hire composition now passes all six combinations of
one-, seven- and fourteen-day contracts with enabled/disabled medical deposits.
The same fixture checks stale offers/funds, read-only storage, unsupported gear,
partial finance/history writes and historical request retries. The 13 affected
native, authority, client and socket tests pass under AddressSanitizer with
allocator mismatch detection enabled
(`/tmp/ja2-hiring-targeted-asan-final.log`, 4.56 seconds).
Installed hiring through pending-arrival checkpoint creation now passes the
bounded two-client scenario below. Pending-hire cold restore also passes;
arrival execution remains under qualification. The nine protocol/client/socket CTests
also pass at protocol 15
(`/tmp/ja2-campaign-hire-protocol15-tests.log`, 3.11 seconds). Initial offers
exclude optional equipment: the legacy copy routine can silently omit items,
so paid equipment needs a checked transfer boundary before it can be offered.
Contract renewal remains open.

The pure economy/offer/hire contract is merged in
[PR #847](https://github.com/tais/1vibe13/pull/847), following #836.
Its five contract tests pass normally and under ASan/UBSan; the isolated
foundation retains its parent's protocol 7 and does not enable executable
hiring. The checked native no-gear constructor is in
[PR #846](https://github.com/tais/1vibe13/pull/846), including rejection of
the native `NO_PROFILE` sentinel even when that array row contains AIM metadata.

The reviewed foundations in PRs #835–#848 and #850–#852 are merged into
`architecture/multiplayer-first-repair` after successful CI on their exact heads.
This does not merge the integrated protocol-17 working tree or the development
branch into `master`. Remaining focused PRs receive fresh checks after conflict
resolution before merging.

Initial installed hiring run `ja2-coop-hiring-8qdk86nu` failed before either
client became ready: the new protocol-15 client treated copied protocol-14
reconnect credentials as corrupt. No gameplay input or spending occurred, and
the source campaign and installed inputs remained unchanged. The private
credential loader now recognizes that known older storage layout, verifies
its original checksums and full campaign binding, and allows only the existing
stale-session retirement path for a different server epoch. Old live network
packets remain rejected. Native pending-hire resume classification and the
credential upgrade regressions pass normally and under ASan. The failed run
remains failed.

Fresh installed run `ja2-coop-hiring-ggrgrog0` passes the pending-hire scope. The
shipping server resumes the original return checkpoint, and both rebuilt
clients successfully process their copied older credentials. Nonleader B
selects Grunty (profile 6), cycles the contract choices and confirms a seven-day
no-gear hire through two released Enter presses. The native result creates one
actor (`4:5`) in transit to A9 at minute 2250 and debits exactly $5,500: $4,000
salary plus $1,500 medical deposit. Both clients agree on the five-person
roster, `AlreadyHired` offer and balance change from $19,140 to $13,640; the
leader remains A and time stays paused at minute 1870. Both panel PNGs were
visually reviewed, including the armed confirmation. Physical OS input is not
certified.

All three processes exit zero. The required generation-4/slot-B checkpoint has
five mercenaries, no loaded world, 11,697,352 save bytes and SHA-256
`a6cae3debb56e1e5cb0926c69fb62b4407f8db2b5b91ca73d37d002d00837326`.
Source campaign/evidence and installed inputs remain unchanged. Evidence is in
the run's `hiring-evidence.json` and `/tmp/ja2-coop-hiring-upgrade-pending.log`.
This run stops before arrival; control of the arrived mercenary in battle
remains unqualified. The seven updated native,
resume, contract and credential CTests pass under ASan with allocator mismatch
detection (`/tmp/ja2-hiring-upgrade-final-asan-tests.log`, 6.25 seconds). All 487
installed-helper Python tests pass (`/tmp/ja2-hiring-all-python-tests.log`).
All three applications build and the complete **246 CTests** pass, including
the architecture gate (`/tmp/ja2-hiring-upgrade-final-build.log`,
`/tmp/ja2-hiring-upgrade-final-ctest.log`, 200.53 seconds). This full run includes
the protocol-15 goldens, native pending-hire and no-profile sentinel regressions,
credential migration and installed-hiring helper tests.

Installed cold Resume `ja2-coop-hiring-restore-ysdegk6p` passes from a byte-checked
private copy of that pending-hire checkpoint. Under fresh session
`1284208749367638060`, both clients recover the exact five-profile roster,
contracts, $13,640 balance and `AlreadyHired` offer. Grunty remains in transit
at A9 with minute-2250 arrival, contract end 13740 and no squad/group membership;
the four ordinary mercenaries retain their coherent group. Nine identical paired
observation hashes span 3.15 seconds while paused at minute 1870. This qualifier
permits only argument-free observations and sends no gameplay inputs.
All three processes exit zero and the required generation-7/slot-A checkpoint
has 11,697,352 bytes and SHA-256
`7d421ba13320374d50c35dfa16c1dd410720720563cbdc25be1c5e9bec741d52`.
Source evidence and private state, all referenced source hashes, installed
inputs and tested binaries remain unchanged. Evidence is the run's
`hiring-restore-evidence.json`; the 14 helper regressions are mandatory CTest
coverage. This proves pending-hire restoration, not flight execution.

The subsequent full-flight run `ja2-coop-hiring-l_h2m2p_` remains failed. Hiring
and leader-controlled time resume succeeded, but native enemy group `80:81`
attacked A9 at minute 1875, before the new hire's minute-2250 flight. The server
paused both clients at the same battle decision and marked its preparation
unsupported. Grunty remained in transit with the original single charge.
Clients exited zero; the server exited 2 because that held encounter could not
be checkpointed. Source and installed inputs were unchanged. The headless
pre-battle boundary rejected the defensive enemy-group path and counted pending
hires that already carry the battle sector as their future landing location.
This interruption was retained, not skipped to claim
an arrival pass. Evidence: the run's `hiring-evidence.json` and
`/tmp/ja2-coop-hiring-full-arrival.log`.

The new defensive path validates the arriving enemy union and exact nonzero
force size before using the shared native pre-battle rules. Entry and retreat
exclude `IN_TRANSIT` actors consistently with native combat participation.
Enemy attacks retain forced insertion; the player's Spread choice remains
specific to native encounters that require deployment. Three engine-linked
cases cover ordinary defense, a pending hire and town invasion, including
malformed contexts, cached preparation, missing-map entry and stale identities.
They pass normally and under ASan (0.80 and 1.31 seconds); the existing 19
arrival cases plus the strengthened pending-hire resume classifier also pass
(3.00 and 5.27 seconds). Logs: `/tmp/ja2-defensive-arrival-native-tests.log`,
`/tmp/ja2-defensive-arrival-asan-tests.log`,
`/tmp/ja2-defensive-regressions-normal.log`,
`/tmp/ja2-defensive-regressions-asan.log`. Installed defensive entry and actual
pending-hire arrival remain under qualification.

All three applications and the installed client rebuild with the defensive
path (`/tmp/ja2-defensive-arrival-shipping-build.log`). The 249 non-architecture
CTests pass in 187.17 seconds (`/tmp/ja2-defensive-arrival-full-ctest.log`), and
the architecture gate passes separately
(`/tmp/ja2-defensive-arrival-architecture.log`). The added defensive-retreat
fixture also passes: actual native retreat moves both defenders, increments
only their retreat records and schedules enemy pursuit while preserving the
pending actor and exact delayed-hire event. Repeated requests cannot replay
those effects. All four defensive cases pass under ASan (1.86 seconds), and
the four native cases plus both new installed-helper CTests pass normally
(1.30 seconds): `/tmp/ja2-defensive-retreat-asan-tests.log` and
`/tmp/ja2-defensive-retreat-tests.log`.

Two subsequent installed defense runs retain failed outcomes:
`ja2-coop-defensive-entry-imwg47d2` and `ja2-coop-defensive-entry-bujn78p1`.
Both prove nonleader entry into the native A9 defense with four defenders and
Grunty still excluded in transit. The first qualifier attempted movement during
the enemy turn; its replacement waits for a native player action window. The
second uncovered a client bug after native combat killed actor `1:2` (profile
238): native corpse removal retained the actor identity but removed its grid and
render pose, and both clients rejected that canonical dead state as an invalid
position. Neither run proves two-owner movement or successful battle completion.
Their source campaigns, installed inputs and failed evidence remain unchanged.

The presentation directory now releases proxies for canonical unposed dead
actors at `NOWHERE`, while retaining posed dying actors and rejecting malformed
or living invalid positions. Identity, duplicate-slot and capacity validation
still precede removal. Proxy rollback, cleanup and slot reincarnation pass the
two focused CTests normally and under strict ASan (1.52 and 2.53 seconds):
`/tmp/ja2-dead-actor-presentation-tests.log` and
`/tmp/ja2-dead-actor-presentation-asan-tests.log`.
The installed qualifier keeps its initial four-living-defender requirement and
accepts subsequent roster/ownership changes only with exact native death
evidence. If either player loses all owned survivors, its movement scope fails.
This change does not certify corpse rendering or completion of the battle.

Installed rerun `ja2-coop-defensive-entry-lw0aiv72` retains a failed overall
result, but its logs prove the dead-proxy correction: both clients materialize
world 1, observe four living posed defenders at revision 8, and later accept
the canonical unposed dead actor through revision 247 without a presentation
rejection. The qualifier discarded the initial living observation because
native interrupt resolution still blocked commands; by the time it checked
entry, combat had killed a defender. Initial visual/roster entry proof and the
subsequent wait for legal player actions are being separated. Neither movement
nor victory is certified. Server/client exits are 2/0/0 and all source, input
and binary immutability checks pass. Original failed evidence remains intact.

Fresh installed run `ja2-coop-defensive-entry-zeqxmcxm` **passes defensive entry
and both owners' movement**. Leader B advances time; nonleader A chooses Forced
entry into the real A9 defense. Both clients first observe all four living
defenders, then native AI kills actor `1:2` (profile 238). Both accept its
canonical unposed dead state through revision 956, and native death cleanup
sets assignment 57/group 0 without losing the other actors. Ordinary player
turn 4 arrives naturally. Owner A moves actor `2:3` (AP 83→71), and owner B moves
actor `0:1` (AP 93→81); each command is Applied and its final position agrees on
both replicas. No interrupt pass was needed in this run; that helper path has
unit coverage only. Time stays at minute 1875, balance at $13,640 and Grunty at
the exact pending minute-2250 flight/end-13740 contract.

Both clients exit zero. The active-battle server stop exits 2 at the expected
ineligible checkpoint boundary and retains the exact prior generation-6/slot-B
checkpoint (SHA-256
`fb50fd7af1e2e39dd9505a7e7f503ace1a3084751cb119029388df3a52cd82dd`).
All source, installed-input and binary immutability checks pass. Evidence is
the run's `defensive-entry-evidence.json` and
`/tmp/ja2-coop-defensive-entry-split-readiness.log`. Both pre-battle decision
captures were reviewed; tactical/corpse paint, physical OS input, battle victory
and the hired mercenary's arrival are not certified by this run. The updated
defensive/turn/movement/restore helpers pass 136 Python tests.

Checked established-campaign AIM arrival now validates the exact due event,
full event parameter, actor identity, pending contract and squad/group absence
before invoking the shared native arrival implementation. The lower boundary is
in [draft PR #848](https://github.com/tais/1vibe13/pull/848). The integrated runtime
explicitly owns when checked arrival becomes active, preserving initial-starter
behavior until that bootstrap completes. Native one-, seven- and fourteen-day
arrival fixtures and actual fixed-step event dispatch pass normally and under
strict ASan (0.69 and 1.15 seconds), including exact one-time retirement and
rejection of a parameter with an added 65536 before narrowing:
`/tmp/ja2-aim-arrival-event-dispatch-tests.log` and
`/tmp/ja2-aim-arrival-event-dispatch-asan-tests.log`.
John's distinct native arrival behavior remains outside the offered paid path.
These engine-linked tests do not replace installed flight/battle qualification.

The native campaign host now irreversibly records dedicated simulation failure
([draft PR #849](https://github.com/tais/1vibe13/pull/849)).
The generic tick dispatcher catches sink exceptions, so the application also
checks that failure before native screen/command work; runtime admission,
publication and checkpoint paths reject the partial state. An actual failing
Lua campaign event proves that later events remain untouched, subsequent ticks
cannot replay it and the frame cannot commit or save. Single-player and legacy
PvP controls retain their previous behavior. All three native failure modes pass
normally and under strict ASan, and the architecture gates pass
(`/tmp/ja2-checked-arrival-dispatch-architecture.log`).

The integrated arrival, failure and dead-proxy changes build in all three
applications and the installed driver. The complete **257 CTests** pass,
including architecture boundaries (257.31 seconds):
`/tmp/ja2-coop-campaign-failure-shipping-build.log`,
`/tmp/ja2-coop-campaign-failure-all-build.log` and
`/tmp/ja2-coop-campaign-failure-full-ctest.log`.

The pending-hire Resume classifier now separately validates completed native
deaths retained in the active roster. It requires exact identity/profile binding,
native death assignment/flags, completed death UI, a visited stationary sector
and no live or stale same-slot squad/group membership. At least one validated
living ordinary mercenary must remain. Bare zero health or contradictory death
markers cannot bypass the original strict pending-hire proof. A real native
skull-completion fixture removes heap-owned squad/group membership, appends one
death-history row, retains the AIM contract and historical profile life, and
leaves Grunty/event/funds unchanged. Repeated cleanup and read-only inspections
cannot replay those effects. The fixture, existing Resume classifier and pure
policy tests pass normally (7.69 seconds) and under strict ASan (1.15 seconds).
This is engine-linked evidence; a completed-battle disk Resume with that mixed
roster remains to be run.

The rendering-protocol foundation also exposed a lighting-only packet bug:
the decoder still assumed a five-byte minimum event, while `LightingChanged`
uses three bytes. The minimum now follows that actual event size. A standalone
29-byte delta and its full co-op envelope pass through decode and the replica,
while stale, invalid and no-op lighting updates leave committed state unchanged.
The normal codec/replica/socket tests and new native battle-history reader test
pass (`/tmp/ja2-lighting-only-delta-tests.log`, 4.55 seconds). The history reader
requires a new exact native town-defense victory record at the battle sector/time;
it is a witness for the forthcoming battle qualifier, not an installed victory.
The three affected codec/replica/socket tests also pass under strict ASan
(`/tmp/ja2-lighting-only-delta-asan-tests.log`, 5.21 seconds). All three game
applications and the separately built installed driver rebuild with both the
completed-death Resume and lighting corrections; architecture boundaries pass
(`/tmp/ja2-coop-death-resume-lighting-all-build.log`,
`/tmp/ja2-coop-death-resume-lighting-driver-build.log`,
`/tmp/ja2-coop-death-resume-lighting-architecture.log`). These latest binaries
have not yet completed the battle-to-campaign qualification. The full **259
CTest** suite now passes with these corrections (204.59 seconds), including
architecture boundaries (`/tmp/ja2-coop-death-resume-lighting-full-ctest.log`).

The explicit `--finish-battle` mode in the installed defensive-entry runner now
uses bounded ordinary owner FIRE/RELOAD/ENDTURN and exact interrupt-pass commands.
It requires confirmed ammunition/AP evidence from each owner, an independently
recorded native defense win, the required cold checkpoint, both clients'
reconnection and coherent surviving/dead/pending roster before clean shutdown.
Unknown final receipts at world drain remain unattributed and cannot be replayed.
Its 27 boundary tests pass. All 582 installed-helper Python tests also pass
(`/tmp/ja2-coop-installed-helper-arrival-cohort-tests.log`, 6.35 seconds).
The arrival witness now also requires unchanged surviving/dead teammate rows
and group membership; ten counterexamples passed the previous weaker witness
and fail the corrected one. Creating a new squad for the arriving hire remains
valid. These are qualifier checks, not installed arrival evidence.
The separate `full_engine_coop_installed_victory_restore.py` runner prepares a
read-only fresh-process Resume check of that eventual save, including reciprocal
live roster/group bindings, exact retained contracts/funds, immutable native
battle/death history and a paused hold. Its 14 tests and all 34 related restore
tests pass, including source contribution-to-journal checks; neither runner is
installed victory or post-victory Resume evidence until a fresh run succeeds.

The first complete battle attempt, `ja2-coop-defensive-entry-bngwaql8`, is
preserved as **failed** after its 600-second bound. Both owners completed native
movement and independently confirmed FIRE: actor `0:1` consumed one round and
31 AP (receipt revision 970), and actor `2:3` consumed one round and 24 AP
(revision 999). A third shot by `3:4` received Applied at revision 1027, but the
native interrupt remained Resolving through revision 3689; the qualifier did not
certify settled completion or replay it. Both replicas agreed. The preserved
native history records profile 238's death and no battle win. Clients exit 0/0;
server exit 2 correctly refuses an active-battle checkpoint. Generation 6 slot B
at minute 1870 remains byte-identical, SHA-256
`178d7e2177cfdd59b899aac545d1fbf5edefd1e58b3f9c087d0a598935718f54`.
All source/assets/binary immutability checks pass. The server trace completes
the third attack using UI-selected actor 0, despite shooter 3.

The native regression now identifies the stall: hit script 472 readies the
enemy's rifle outside its turn, replacing the shooter's pending AFTERSHOT with
BEFORESHOT. The real completion callback drains its combat counter but cannot
resolve the replacement kind. Both regression modes fail against the old
production implementation (0.72 seconds) and pass with interrupt accounting
restricted to the current team's actions. Native AP costs remain unchanged,
off-turn observers receive no interrupt credit, and normal player/enemy actions
and legacy network accounting retain their controls. Both cases pass in the
isolated PR checkout (0.67 seconds) and under strict ASan in the integrated
checkout. Deferred attack ownership still selects the UI actor and remains a
separate defect; the fixture does not claim to correct it. Installed battle
completion must still be demonstrated after this fix.

The isolated portable rendering foundation is published as
[merged PR #850](https://github.com/tais/1vibe13/pull/850). Its independent review,
normal/ASan codecs and replica tests, native compilation, credential migration,
installed SDK consumers and architecture checks pass. That branch advances its
older snapshot/delta/admission versions to 9/8/9 and the public SDK to 0.3;
the integrated working tree now uses protocol 16, snapshot 12 and delta 11.
The SDK upgrade also migrates an existing 0.2.0 default cache to 0.3.0 and rejects
incompatible version overrides. Linux CI caught a brittle negative SDK test
that expected CMake's rejection text to contain a specific phrase. The corrected
test checks that the exact installed 0.3.0 package is considered and rejected
for a 0.2 request; accepted-current and missing-package counterexamples fail.
Normal and strict-ASan SDK tests pass; all 15 GitHub checks passed on
`761cd76ae` before merge. The SDK-only subset is also integrated locally.

The native capture layer is isolated in
[merged PR #852](https://github.com/tais/1vibe13/pull/852), following #850.
It captures actual native pose, bounded palette names, portrait aliases and
applied camouflage, plus world lighting. The engine-linked fixture exercises
malformed native data, visibility filtering, completed-death omission and
transactional failure; it fails against the parent implementation. All three
affected native tests pass normally (11.50 seconds) and under strict ASan
(15.45 seconds), with portable tests, all three application translation units
and architecture checks also passing. All 15 GitHub checks passed before merge. This does
not activate the client renderer or qualify installed tactical painting.

Windows CI found that the native campaign failure host redeclared
`gfProgramIsRunning` with C++ linkage even though its canonical declaration in
`sgp.h` has C linkage. [Draft PR #849](https://github.com/tais/1vibe13/pull/849)
now includes that header and guards against the conflicting declaration.
Five native tests pass normally and under ASan/UBSan in its isolated worktree.
All 15 checks passed on the linkage fix; integrating the newly merged development
branch then required preserving both conflicting CI and architecture sections.
Fresh checks are now running on `fd63f8ad5`. The integrated shipping applications
have been rebuilt with the same correction.

An older development CI run also exposed a coordinator-test port collision.
[Merged PR #851](https://github.com/tais/1vibe13/pull/851) retries actual test
listener binds only for the exact native address-in-use error and publishes the
successfully bound endpoint before clients connect. A held first listener makes
the collision deterministic, including reuse-disabled fixture endpoints on
Windows; shipping endpoint defaults remain unchanged. The root integration
preserves its additional pre-admission transport capacity and passes all three
affected normal/strict-ASan tests. Both new Python CTests also pass
(`/tmp/ja2-coop-port-root-tests.log`, 17.02 seconds;
`/tmp/ja2-coop-port-root-asan-tests.log`, 17.41 seconds).

The first installed cold-resume attempt exposed periodic checkpoint starvation:
each cold checkpoint disconnected clients and superseded their transfer, so slow
loads repeatedly reached an overdue timer before campaign interaction. Optional
checkpoints now defer while any connection is present, including admission and
synchronization. Required bootstrap, post-battle and shutdown checkpoints retain
their existing drain and native eligibility checks. Periodic saving during a
connected session remains unfinished; it needs a checkpoint design that does not
force every client to reload. The isolated fix is available in
[merged PR #837](https://github.com/tais/1vibe13/pull/837); its listener lifecycle
regression and full architecture checks pass against the clean protocol-7 base.

The ordinary worldless co-op Screen now offers group selection with `Tab`/`]`
and `[`, one adjacent-sector choice with arrow keys, and `Enter`/`Esc` confirmation
or cancellation. Pending wilderness notices use `A` to acknowledge the final
destination or `S` to stop an interrupted route. A sole prepared battle exposes
`E` for the permitted forced/Spread entry and `R`, then `Enter`, for permitted
retreat. Unsupported native choices remain unavailable. Time and shared-action
requests exclude each other while pending, and stale observations invalidate
armed selections. These controls submit requests; clients do not move groups,
advance campaign time or load authoritative encounters locally.

Before the map/hiring follow-up, all **232 CTests**, **464 installed-helper Python
tests** and all three application builds passed. The new action codec/authority, input,
client-core and two-socket tests also pass under AddressSanitizer. The two real
socket clients prove nonleader success, a competing stale request without a
second execution, historical duplicate delivery, and time/travel serialization.
Logs: `/tmp/ja2-campaign-action-full-ctest.log`,
`/tmp/ja2-campaign-action-python-tests.log`,
`/tmp/ja2-campaign-action-final-build.log`,
`/tmp/ja2-campaign-action-asan-tests.log`.
After the connected-checkpoint fix, the listener, socket, input, checkpoint
eligibility and installed-runner tests pass again, as does the architecture gate
(`/tmp/ja2-campaign-action-checkpoint-tests.log`,
`/tmp/ja2-campaign-action-checkpoint-architecture.log`).

After the map and hiring-policy follow-up, all **233 CTests**, **470 Python
helper tests** and all three application builds pass. Quote and willingness
policies also pass under AddressSanitizer and UndefinedBehaviorSanitizer.
Logs: `/tmp/ja2-coop-shared-campaign-final-ctest.log`,
`/tmp/ja2-coop-shared-campaign-final-python.log`,
`/tmp/ja2-coop-aim-willingness-apps-build.log`,
`/tmp/ja2-coop-aim-policy-asan-tests.log`.
The subsequent bounded arrival-dialogue settlement check passes its focused
51 Python tests; the full run above predates that qualifier change.

The installed two-client run `ja2-coop-campaign-actions-00o_w4wi` now verifies
the native command pipeline. Both clients remained connected across the normal
30-second checkpoint interval: 39.111 seconds, 71 matching paused-state samples,
and no reconnects. The nonleader received an exact `Applied` receipt for adjacent
A9-to-A10 travel; the time leader advanced the native clock; the nonleader then
received an exact `Applied` receipt for Spread battle entry. The server entered
A10 and both clients received assigned tactical replicas. Installed input
fingerprints remained unchanged.

That run does **not** qualify playable battle entry: both clients rejected the
stock A10 map's trailing legacy editor bytes before tactical rendering. Its
evidence now records `passed=false` and retains the successful native steps. The
runner requires exact-map validation and tactical-presentation readiness before
it may pass. Both arrival screenshots were visually reviewed: the clock, leader,
prepared encounter and entry/retreat controls are visible. They do not establish
physical input or tactical rendering acceptance.

The passive map parser now retains legacy editor leftovers in an explicit inert
span after validating every declared section. It also accepts unused legacy
edgepoint split indices while retaining count and byte bounds. Neither is
replayed into client gameplay or geometry; modern formats still require exact
EOF and canonical split indices. A read-only census parses all 261 installed
stock maps (previously 82 passed, 175 had leftover bytes and four had stale split
indices). Synthetic regressions compare every rendered grid/layer with and
without these legacy artifacts and retain truncated/count rejection. All three
applications build and the five affected parser, world-host, geometry, scrolling
and runner tests pass. The parser, world-host and native geometry tests also pass
under AddressSanitizer, and the full architecture gate passes.

Fresh installed run `ja2-coop-campaign-actions-oznuayi5` passes the stronger
qualification. It starts from a private seeded cold checkpoint, holds two ready
clients for 39.172 seconds (71 matching samples, no reconnect), accepts nonleader
A9-to-A10 travel, advances time only through the leader, and accepts nonleader
Spread entry. Both clients validate the exact A10 map and reach tactical
presentation readiness. Each peer then moves a different assigned mercenary;
both moves receive `Applied`, settle at the same positions on both replicas and
pay native combat AP costs. Both arrival-screen PNGs were reviewed; tactical
paint and physical OS input are not separately certified. Installed inputs are
unchanged. The run's `campaign-action-evidence.json` and
`/tmp/ja2-coop-campaign-actions-run.log` retain the evidence. This qualifies the
seeded travel-to-playable-battle path, not victory/return or the full campaign.
The earlier failed run remains failed.

Reproduce the opt-in installed scenario with matching development binaries:

```sh
cmake --build build --target full_engine_coop_installed_entry full_engine_coop_installed_client
python3 tests/full_engine_coop_installed_campaign.py \
  --server-executable build/full_engine_coop_installed_entry \
  --client-executable build/full_engine_coop_installed_client \
  --installed-data-root /path/to/JA2 --battle-choice enter
```

`--battle-choice retreat` instead exercises explicit remote retreat, native
return travel and a required cold checkpoint. Installed run
`ja2-coop-campaign-actions-wyqo2tfr` passes this path: 39.158 seconds of stable
ready-cohort observation, nonleader travel and retreat receipts, native A10-to-A9
return, then a fresh leader pause after the arrival quote releases its pause
lock naturally. Both clients agree on the four mercenaries, idle A9 group and
112200 world seconds. Server and clients exit zero; the new generation-3 cold
checkpoint has a validated manifest, exact save SHA-256 and a portable header
with four mercenaries and no loaded world. Installed inputs are unchanged.
The separate shipping cold-restore run
`ja2-coop-campaign-restore-q1o7f0ya` also passes. It copies the validated saved
server and client state into fresh private directories, starts the shipping
`JA2_ENGLISH` executable with normal Resume, and restarts both clients. Their
live observations restore 112200 world seconds and the exact profile,
assignment, group-sector, transport and empty-route values under a fresh server
session. Both clients and the server exit zero; the server writes generation 4
with a validated manifest/save/header. The original source evidence/private
state and installed assets remain unchanged. No gameplay input was sent. This
qualifies the actual saved idle return state, not wounded/travelling rosters or
active-battle persistence. Restore artifacts: the run's
`campaign-restore-evidence.json` and `/tmp/ja2-coop-campaign-restore-run.log`. Logs: `/tmp/ja2-coop-campaign-retreat-run4.log` and
the private run's `campaign-action-evidence.json`.

Earlier retreat run `ja2-coop-campaign-actions-lsnq5e39` completed the same native
journey but attempted shutdown during the arrival quote and correctly failed
checkpoint eligibility with `dialogue active`. It remains failed. The runner
now waits for the native pause lock to clear within its original timeout, then
uses the leader's ordinary pause control; it does not clear native dialogue or
weaken checkpoint eligibility. All runs use private campaign/process directories
and check installed-input fingerprints.

To repeat the cold-load qualification from a successful retreat run:

```sh
python3 tests/full_engine_coop_installed_campaign_restore.py \
  --source-evidence /path/to/retreat-run/campaign-action-evidence.json \
  --server-executable build/JA2_ENGLISH.app/Contents/MacOS/JA2_ENGLISH \
  --client-executable build/full_engine_coop_installed_client \
  --installed-data-root /path/to/JA2
```

The two campaign qualifier CTests pass, and the complete Python helper suite now
passes **476 tests** (`/tmp/ja2-campaign-restore-probe-tests.log`,
`/tmp/ja2-campaign-restore-all-python.log`). The previously reported 233 CTests
predate the new native willingness and restore tests.

The pure observations and campaign-command layer is available for review in
[draft PR #836](https://github.com/tais/1vibe13/pull/836). That isolated foundation
retains the clean base's protocol 7 and excludes native runtime/UI integration;
the integrated working tree uses protocol 15. The updated foundation supports
260 aggregate group members; its codec passes at the exact limit and rejects
261, with normal and ASan/UBSan coverage.

Checked salary/equipment/deposit quotes are available in
[draft PR #838](https://github.com/tais/1vibe13/pull/838) and the integrated tree.
They reject invalid contract lengths and amounts beyond the signed native
finance range without changing the existing laptop API. Boundary tests pass.
The existing AIM UI now shares a pure willingness decision in
[draft PR #839](https://github.com/tais/1vibe13/pull/839) and the integrated tree.
It preserves morale, first-hated-merc, buddy, tolerance, learned-relationship and
death-rate/reputation precedence, while rejecting absent profile IDs before
lookup. Normal/sanitized tests and 98,304 old/new wrapper comparisons pass.
All three integrated applications rebuild, and quote/willingness/runner CTests
pass. The shared native fact reader is now available in
[draft PR #842](https://github.com/tais/1vibe13/pull/842), stacked on #839, and the
integrated tree. The laptop retains dialogue effects while the reader returns a
transactional willingness decision from current native profiles/team/risk state.
Its real native regression passes normally and under ASan/UBSan; the 98,304
wrapper comparisons still pass, as do integrated and isolated architecture
checks. Logs: `/tmp/ja2-native-aim-willingness-tests.log`,
`/tmp/ja2-native-aim-willingness-asan-tests.log`,
`/tmp/ja2-shared-aim-willingness-equivalence.log`,
`/tmp/ja2-native-aim-willingness-architecture.log`.
Checked native event scheduling is available in
[draft PR #843](https://github.com/tais/1vibe13/pull/843). It detects queue and
timestamp failures without invoking legacy assertion UI. The integrated
applications, 21 affected native tests and its sanitizer regression pass.
Checked finance/history writers are in
[draft PR #844](https://github.com/tais/1vibe13/pull/844); the initial integrated
applications, six native/architecture CTests and ledger sanitizer test pass.
The follow-up read-only append preflight checks writer provenance and aggregate
row capacity before a hire starts; its isolated native and sanitizer tests pass.
The dedicated assertion boundary is in
[draft PR #845](https://github.com/tais/1vibe13/pull/845). It throws before error
screens, rendering or assertion autosaves, preserving the legacy path outside
dedicated co-op. Its three native cases pass in the integrated tree and under
sanitizers in the isolated PR. The installed paid-hire scope above uses these
foundations; delayed arrival execution still needs qualification.

Exact map identity is isolated in
[draft PR #840](https://github.com/tais/1vibe13/pull/840), with native filename
capture, snapshot/delta preservation and replica validation. Its intermediate
protocols are global 8 / snapshot 8 / delta 7; the integrated tree remains
15 / 12 / 11. Its 21 local CTests, sanitizer and architecture checks pass. The
passive parser is available in stacked
[draft PR #841](https://github.com/tais/1vibe13/pull/841), based on #840. Its public
parser passes all 261 stock maps in both normal and ASan/UBSan runs while
preserving their exact bytes and contiguous spans. The child does not add a
protocol change. Native rendering is outside both isolated PRs.

This records source implementation and the bounded installed evidence above,
not complete two-client acceptance. The
dated evidence below retains the scope of each earlier run. Multi-leg routes,
crossing-group rescheduling, manual deployment, autoresolve and unsupported
arrival contexts remain open. Hiring arrival, contract renewal, other spending,
remaining actor-ownership workflows, meaningful travelling/wounded save/resume
qualification and active-battle persistence also remain required. A completed
travel/battle/return campaign workflow must still be qualified with installed
data before this checklist can mark it complete.

## Evidence already available

Latest qualification (2026-09-07): all **229 CTests** and **455 installed-helper
Python tests** pass. All three applications and both opt-in installed drivers
build. The 21 native travel/arrival cases also pass under AddressSanitizer.
Final logs: `/tmp/ja2-coop-retreat-final-ctest.log`,
`/tmp/ja2-coop-retreat-python.log`, `/tmp/ja2-coop-retreat-final-build.log`,
`/tmp/ja2-coop-retreat-final-asan-tests.log`.

- Native headless retreat (2026-09-07) is now an explicit internal battle
  action. It shares the local button's mercenary records, group movement,
  loyalty and enemy response. Native ambush/bloodcat/concealed/just-retreated
  prohibitions still apply. The sole exact prepared decision, group/member
  identities, route ownership, prior sectors, fresh native movement costs and
  existing events are validated before mutation. Vehicles, unsupported
  assignments and militia requiring an autoresolve continuation reject without
  being silently resolved. Success consumes the decision and leaves time
  paused. Native event processing, including the actual sector Lua callback,
  completes the return journey after an explicit resume.
  Native event-queue failures used to enter the assertion renderer; during a
  headless battle action they now unwind to its fail-stop owner. Tests inject
  failure at both the arrival event and the subsequent warning allocation;
  neither may replay records, movement or RNG, or advertise actionable choices.
  Stale decisions, replaced actors, cyclic/aliased routes and another queued
  continuation are also covered. All 21 native travel/arrival scenarios pass
  normally and under AddressSanitizer. Installed seeded retreat runs
  `ja2-coop-native-entry-67bgt805` and `ja2-coop-native-entry-6vxsldmw` (ASan)
  both pass with two actors departing A10 toward A9, an unchanged paused clock,
  native enemy response, no loaded map or dialog, and no duplicate side effects.
  Both exit cleanly; installed input fingerprints remain unchanged. Logs:
  `/tmp/ja2-coop-retreat-native-tests.log`,
  `/tmp/ja2-coop-retreat-asan-tests-2.log`,
  `/tmp/ja2-coop-retreat-installed-2.log`,
  `/tmp/ja2-coop-retreat-asan-installed.log`.
  This is **not remote retreat, two-client campaign travel or complete co-op**.
  The runtime action remains gated to established strategic state; shared
  travel/battle authorization and client controls are still separate work.
- Pending native arrivals now reach both clients (2026-09-07). Campaign status
  carries the exact front decision, sector, queue count and cached preparation
  stage, including native battle options and participant counts only when
  prepared. NPC, coordination, reinforcement-required, unsupported and failed
  contexts are distinct. Capture is read-only: no native group traversal,
  preparation, RNG, clock advancement or remote authority is hidden in it.
  Canonical empty replacements clear resolved notices; server and client
  decision high-water marks prevent a consumed ID from reappearing in a newer
  status revision. Arrival changes invalidate old time-control requests, and
  even the time leader cannot resume the clock through a pending decision.
  The existing waiting screen explains the native hold and explicitly says
  remote decision controls are not yet available. Protocol remains 13 with
  matched development builds; status width is now 96 bytes, not a new version
  or compatibility shim.
  The opt-in `full_engine_coop_installed_arrival.py` fixture uses normal runtime
  admission/status delivery around a seeded native prepared A10 encounter.
  Two actual clients received matching state, retained it on reconnect and
  cleared it after server loss. Both native PNGs in
  `ja2-coop-arrival-observation-7r8lkg94` were visually reviewed for text/layout.
  The updated binaries also pass observation, reconnect and disconnect clearing
  in `ja2-coop-arrival-observation-f5o_93f2`; both of its native PNGs were also
  visually reviewed. The scripts themselves do not claim physical-input or
  visual acceptance automatically.
  The runner waits for native screenshot completion rather than mistaking a
  newly created zero-length file for a finished image. Installed input
  fingerprints are unchanged. Evidence:
  `/tmp/ja2-coop-arrival-observation-installed-final.log`,
  `/tmp/ja2-coop-arrival-final-installed.log`.
  This qualifies observation and rendering, **not remote choices, physical
  input, travel, battle entry with peers, or a complete campaign**.
- Installed ambush entry crash fixed (2026-09-07). Native enemy encirclement
  put the `+100` forced-insertion marker into the creation record's live facing,
  desired direction and AI direction. A real installed ambush Spread entered
  the structure table with direction 103 and crashed. Encirclement now creates
  an ordinary 0–7 facing and applies the marker only to deployment state before
  insertion. The shared structure lookup bounds both the octant and the
  selected file variant before changing the existing footprint. This fixes
  the native path used by co-op without introducing separate ambush rules.
  A mandatory data-free `native_actor_structure` regression exercises invalid
  direction bytes, undersized/missing tables and the queen's special variant.
  The corrected installed fixture forces encirclement and checks both mercs,
  all seven enemies, their live/desired/AI/insertion directions and render nodes.
  `ja2-coop-native-entry-tniqknro` passes ordinary Spread, forced ambush,
  leadership-enabled ambush Spread and real empty-map load failure; duplicate
  attempts cannot reload or reroll. Log:
  `/tmp/ja2-coop-arrival-ambush-entry.log`.
  The ambush Spread also passes installed-data AddressSanitizer qualification
  (`ja2-coop-native-entry-d3ybzz2u`,
  `/tmp/ja2-coop-arrival-ambush-asan-entry.log`). The structure regression adds
  all eight valid native octants and the queen's one-variant positive control;
  both normal and ASan builds pass those checks.
  Queued native message boxes now block NPC replies and preparation/entry,
  including after the real loader has run. Unexpected dedicated native dialogs
  are logged, not automatically answered. The original fixture created enemies
  in a player-occupied sector and queued an invariant warning; its earlier
  apparent qualification is superseded by the corrected enemy-first fixture.
  Qualification at this step: all 226 CTests and 455 installed-helper Python
  tests pass; all three applications and both opt-in drivers build. Logs:
  `/tmp/ja2-coop-arrival-final-ctest-2.log`,
  `/tmp/ja2-coop-arrival-final-python.log`,
  `/tmp/ja2-coop-arrival-final-all-build.log`,
  `/tmp/ja2-coop-arrival-final-drivers-build.log`.
- Explicit native headless battle entry (2026-09-07) now builds on the prepared
  arrival decision. Native forced insertion and the player's explicit `Spread`
  choice remain distinct; ordinary and leadership-enabled ambush deployment
  reuse the normal game's placement helpers without opening its widgets.
  Exact pending/prepared identities, current native context and independently
  owned bounded routes are validated before mutation. Missing real maps reject
  without consuming alternate-map selection or falling back to a placeholder.
  The normal loader still owns squad/path cleanup, entry quests, temp state,
  soldiers, enemies and AI initialization. Success consumes the sole decision;
  partial failure latches and prevents another world load or RNG reroll.
  Runtime entry rejoins existing fresh-baseline admission and victory/drain
  handling, without leaving a loaded encounter labeled strategic idle.
  Engine-linked arrival tests cover invalid choices/context, missing maps,
  stale actors, cyclic/aliased paths and other queued continuations. A new
  opt-in installed-data driver exercises real A10 loading and native placement
  using explicitly seeded, disposable fixtures rather than a remote player.
  Qualification: all 225 CTests pass (224 execution cases plus the corrected
  architecture gate), all 446 installed-helper Python tests pass, all three
  applications and both opt-in installed drivers build. Logs:
  `/tmp/ja2-coop-entry-ctest.log` (224 execution passes; initial architecture
  assertion corrected in the separate final run),
  `/tmp/ja2-coop-entry-architecture-final.log`, `/tmp/ja2-coop-entry-python.log`,
  `/tmp/ja2-coop-entry-full-build.log`, `/tmp/ja2-coop-entry-all-build.log`.
  The original installed run `ja2-coop-native-entry-y2fe4b7o` is superseded:
  it did not reject a queued fixture invariant dialog. Use the corrected
  installed qualification and ambush regression described above. Reproduce using
  `tests/full_engine_coop_installed_entry.py` with the explicit
  `full_engine_coop_installed_entry` target and an installed-data root.
  Remote travel/decision controls and their permissions, manual deployment,
  remote retreat, autoresolve and remaining native contexts are still unfinished.
  This is **not complete co-op or an installed two-client travel playtest**.
  No protocol version change, release, commit or push was made.
- Native headless pre-battle preparation (2026-09-07) now shares the existing
  PBI's participant/wake, ambush/scout, leadership/deployment, record and action
  rules with the local game UI. Coward morale uses the same native rule too.
  The dedicated runtime prepares a supported pending surface on-foot battle
  once, records the native result and continues waiting for a player decision.
  A logical native battle context supplies sector/participant queries without
  claiming a screen or loading a world. Exact retries return the cached result;
  they cannot reroll an ambush, repeat morale/records or reset the ambush radius.
  Arrival captures retreat flags before native cleanup removes them. Invalid
  context rejects before side effects; partial failure or unhealthy simulation
  RNG latches a host failure without publishing a prepared result. Adjacent
  militia requiring a reinforcement choice, coordination, unconscious-only and
  other unsupported contexts remain pending, not silently answered.
  Native scheduled-arrival tests now cover these restrictions, scout prevention,
  leadership-enabled ambush deployment, bloodcats, retained retreat flags,
  concealment, first-battle facts, morale, stale/corrupt context and RNG failure.
  Low-morale quote flags remain native, without queuing a local portrait on the
  headless host. Teardown of an old runtime cannot clear its replacement's
  logical battle context.
  Qualification: all 225 CTests (including 16 native arrival/preparation cases),
  all 446 installed-helper Python tests, all three applications and the explicit
  installed driver pass. Logs: `/tmp/ja2-coop-prebattle-native-final.log`,
  `/tmp/ja2-coop-prebattle-ctest-final.log` (224 tests),
  `/tmp/ja2-coop-prebattle-architecture-final.log` (architecture gate),
  `/tmp/ja2-coop-prebattle-python.log`, `/tmp/ja2-coop-prebattle-full-build.log`,
  `/tmp/ja2-coop-prebattle-driver-build.log`.
  This is **preparation, not battle entry or complete co-op**. Remote decisions,
  their authority policy, deployment and enter/retreat/autoresolve actions still
  need implementation. No new protocol version or release was made, and no new
  installed two-client/physical-input acceptance run has been performed here.
- Headless native arrival decisions (2026-09-07) now replace worldless dedicated
  NPC/battle/coordination dialogs with a bounded runtime-owned pending queue.
  Native detection still decides whether an interaction exists, including NPC
  exclusions and bloodcat ambushes. Unbound single-player/legacy multiplayer
  keeps its local dialog path. Decisions retain exact group/dialog-group
  incarnations, observation time, sector and native encounter code; multiple
  notices retain FIFO order and identical observations coalesce. Capture errors
  and overflow fail closed without dropping accepted notices or choosing an
  outcome. Arrival interrupts at its exact native event time. Native resume
  eligibility, fixed-step advancement and new travel cannot bypass the hold.
  Cold checkpoints cannot lose these currently unserialized continuations.
  The server logs the pending interaction without repeatedly emitting it.
  Internal NPC replies can acknowledge a finished trip or stop a retained route,
  using native path cleanup and leaving time paused. Wrong/duplicate IDs, changed
  native context and stale group incarnations cannot act on another decision.
  Stop validates all member path links/ownership before freeing any route.
  Nine engine-linked scenarios cover native NPC, enemy, bloodcat and coordinated
  arrivals; real fixed-step event interruption and later-event retention; Lua
  liberation effects; two independent notices; retained-route stop; stale group,
  malformed graph/path and bounded-capacity cases.
  Qualification: all 218 CTests, all 446 installed-helper Python tests, all three
  applications and the explicit installed driver pass. Final verification logs:
  `/tmp/ja2-coop-arrival-ctest-native-final.log` (217 tests),
  `/tmp/ja2-coop-arrival-architecture-final.log` (architecture gate),
  `/tmp/ja2-coop-arrival-python.log`, `/tmp/ja2-coop-arrival-full-build.log`,
  `/tmp/ja2-coop-arrival-driver-build.log`.
  This is **not client-enabled travel, battle entry or complete co-op**. Battle
  and coordination decisions remain pending; the later preparation boundary
  above does not implement deployment/actions, remote controls or authority.
  No installed two-client or physical-input/paint acceptance run was performed
  for this boundary. Test-owned Lua content is not installed Arulco content.
- Native adjacent travel commit (2026-09-07) now starts a validated on-foot trip
  without partially mutating a squad when preparation fails. It replans from
  current native state, allocates each member's separate two-node path and the
  waypoint privately, then atomically inserts the native arrival/warning events.
  Existing queue nodes, IDs and FIFO order survive; failed event allocation or
  capacity checks preserve the queue and next identity as well as actor/group
  state. Stale arrival/warning events are explicit conflicts, not deleted.
  The first commit supports one adjacent leg only. Longer routes, crossing groups
  requiring native event/RNG rescheduling, tactical/pre-battle and event-dispatch
  contexts are rejected without movement. Five-minute town trips post no warning.
  Start neither advances time nor consumes RNG. The native fixed-step clock then
  runs the normal arrival callback. The engine-linked regression now completes
  an A9-to-A10 trip for both mercenaries, clears their finished routes and executes
  a test-owned Lua liberation effect exactly once. Real native group observation
  sees the committed transit state. Fault tests cover event-allocation rollback,
  capacity, duplicate starts, event conflicts, stale members and unsupported
  contexts. The standalone batch test passes ASan/UBSan, including failure of
  every preparation allocation and a damaged queue cycle.
  Qualification: all 209 CTests, all 446 installed-helper Python tests, all three
  applications and the explicit installed driver pass. Logs:
  `/tmp/ja2-coop-travel-commit-ctest.log`, `/tmp/ja2-coop-travel-commit-python.log`,
  `/tmp/ja2-coop-travel-commit-apps-build.log`,
  `/tmp/ja2-coop-travel-commit-batch-asan.log`.
  This is an internal native commit, **not client-enabled travel** or an installed
  two-player acceptance run. The read-only Lua fixture is test campaign content,
  not the installed Arulco script. Multi-leg continuation, crossing-group handling,
  headless NPC/pre-battle interaction, subsequent encounter entry, travel controls
  and permissions remain unfinished. Time leadership grants no travel authority.
- Native strategic travel preflight (2026-09-07) now validates an exact live
  idle surface/on-foot group without relying on map-screen selection or changing
  native simulation state. It reuses the native eligibility and pathfinding
  rules, with isolated cost/output state: another squad's weight cache, Shift and
  GUI plotting flags cannot change the server's candidate route. Actual carried
  weight and survival traits still affect travel times. Preview never wakes or
  collapses a mercenary, writes GUI error text, installs paths/waypoints, creates
  events or advances the clock. Existing local plotting behavior is preserved.
  The planner returns every adjacent leg (up to all 255 across the playable map),
  exact members and total native minutes only after validating the destination
  and timestamp bounds. Stale identities, malformed native lists, existing
  movement, hostiles, native assignment/health/fatigue/quest restrictions and
  unreachable destinations leave the previous output unchanged. The museum's
  room-dependent restriction explicitly requires unavailable tactical context;
  it cannot read a null/stale map table or silently waive the restriction.
  The engine-linked `native_coop_travel` regression exercises those cases with
  the real group/actor directories, eligibility rules, search and cost code,
  including legacy Shift/selected-POW positive controls and a 255-leg route.
  Qualification: all 207 CTests (including architecture boundaries), all 446
  installed-helper Python tests, all three application builds and the explicit
  installed driver pass. Logs: `/tmp/ja2-coop-travel-ctest.log`,
  `/tmp/ja2-coop-travel-python.log`, `/tmp/ja2-coop-travel-apps-build.log`.
  No new installed two-client or physical-input/paint run was performed for
  this internal native-planner step.
  This is a native planning API, **not enabled travel orders**, a new wire
  protocol/UI, an installed sector-travel certificate, or complete co-op.
  The later single-leg commit above supersedes this step's commit gap; broader
  travel/encounter entry and permissions remain unfinished.
- Live friendly strategic groups (2026-09-07) now have an independent,
  server-authoritative observation channel. Both passive clients receive exact
  group/mercenary incarnations, live profiles/assignments, occupied sectors,
  vehicle/transport flags, the next transit sector and its native arrival time,
  plus the final native waypoint. Empty persistent squads and enemy groups are
  excluded. The final waypoint is not a full route or whole-trip ETA.
  The current maximum replacement is 12,352 bytes, bounded to 255 groups/260
  members after the 2026-09-22 correction for all six supported vehicle slots;
  malformed identities, duplicate membership, impossible coordinates and native
  list cycles cannot produce a partial roster. Unavailable is an explicit
  replacement; disconnect clears the cache and a new transport receives a fresh
  snapshot. Native capture failures report a bounded diagnostic and unavailable
  data instead of failing the running campaign. The current development protocol
  version is unchanged. This adds a copied runtime view and a read-only `GROUPS`
  test probe, not a second strategic UI or enabled travel orders.
  Data-free tests cover real native group/actor directories, stale slot reuse,
  live versus persistence-only profiles, native list faults and unchanged paths/
  clocks. Codec, client and actual loopback transport regressions pass; the codec
  also passes ASan/UBSan including every single-bit mutation of its fixture.
  Qualification: all 206 CTests and 446 installed-helper Python tests pass;
  all three applications and the explicit installed driver build. The installed
  two-client run passed create/Ready/private reconnect/checkpoint/cold resume,
  A/B movement, shared group/clock observations, retained time leadership and
  stale-state cleanup. Both clients reported group `30:31` at A9, revision 2,
  with the same four exact mercenary identities/profiles, checked against each
  client's tactical roster and their disjoint assigned-actor sets. Reconnect
  restored that unchanged revision; shutdown returned only `ready:false` from
  each group getter. This installed case was stationary strategically: transit
  fields/final waypoints are native-fixture tested, not an installed travel or
  arrival certification. No new physical input, paint, strategic-speed or
  leader-loss strategic-pause claim. All owned test processes stopped and
  protected installed inputs were unchanged.
  Logs: `/tmp/ja2-coop-groups-ctest.log`,
  `/tmp/ja2-coop-groups-python-final.log`, `/tmp/ja2-coop-groups-installed.log`.
  Evidence root:
  `/var/folders/9y/v1xx2nbd5jg8pt7ntqtndr0w0000gn/T/ja2-coop-installed-smoke-1pelln1n`
  (`campaign-groups-evidence.json`, `campaign-status-evidence.json`, client/server
  logs). Reproduce with the existing installed smoke command plus
  `--gameplay-client-executable .../build/full_engine_coop_installed_client
  --verify-campaign-status --verify-campaign-groups --keep-artifacts`.
  Sector travel, encounter entry and travel permissions remain separate
  unfinished work; the time leader was not silently granted travel authority.
- Strategic time controls (2026-09-07) are implemented end to end, separately
  from tactical commands. Only the campaign-ready designated leader can request
  pause, 5, 30 or 60 campaign minutes per real second. The interim strategic
  waiting screen exposes `P` / `1` / `2` / `3` with pending/terminal feedback;
  this is not yet a native strategic-map clock widget or sector-travel UI.
  Controls require an established, worldless strategic host on the native map
  screen. Tactical combat, startup/helicopter arrival, world draining and loaded
  worlds cannot be paused or compressed through this path. Native pause locks,
  dialogue, battle and usable-roster checks still apply, without opening a
  headless GUI error dialog. The existing fixed-step scheduler processes time;
  the command itself neither warps time nor clears event interrupts.
  Strategic time pauses when leader loss/unreadiness is detected (also on
  voluntary leader retirement); readiness returning never resumes it. This is
  not a new tactical disconnect-pause policy. Explicit designation/handover and
  leadership persistence across server restart remain open.
  A separate control revision ignores ordinary clock ticks but invalidates
  older resumes on control-state changes. Eligible requests consume it before
  native execution, including native refusals. Exact duplicates return the
  historical result. One bounded result per authenticated transport is reserved
  before mutation and retained through backpressure/same-connection resync;
  results follow the server's clock observation. Reconnect never replays a
  potentially applied resume. No protocol-version bump or legacy compatibility
  path was added.
  Qualification: data-free tests exercise the real native clock and scheduler,
  all three rates, native locks/startup/battle/roster failures, loaded-world and
  tactical exclusions, and subsequent pause stability. Pure authority, codec,
  physical-down/up latch and client tests cover stale/duplicate commands,
  leadership/readiness gates, transport replacement, bounded result retention
  and malformed replies. Production loopback transports cover authenticated
  request queuing and ordered status/result delivery. This does **not** claim a
  new installed two-player strategic-speed/leader-loss acceptance run, physical
  keyboard/paint verification, travel, or full co-op completion.
  All three application variants and the explicit installed driver build;
  all 204 CTests and 436 installed-helper Python tests pass. The new pure
  time-control protocol/authority/input suite also passes ASan and UBSan.
  The final input-owner latch cleanup and exact higher-rate scheduler checks
  are covered by focused retests. Logs: `/tmp/ja2-coop-time-verified-tests.log`,
  `/tmp/ja2-coop-time-python.log`, `/tmp/ja2-coop-time-focused-final.log`.
- Initial live campaign status qualification (2026-09-07) was a read-only,
  session-bound server observation. The native tactical view displays the
  server day/time, phase, native pause flag, time leader and ready-player count;
  the strategic waiting screen can show the same observation without a tactical
  map. That installed run did not exercise time-speed, travel or strategic-map controls.
  The development default is the first observed campaign-ready player (lowest
  identity for a same-pump tie). Disconnect, retirement and tactical-world
  changes retain that identity; reconnect restores readiness. A new server
  session chooses afresh. Explicit designation, handover and durable leadership
  across server restart are not implemented. The strategic time-control entry
  above supersedes this initial status-only implementation's offline-pause gap.
  No hiring/spending/mercenary-command permissions follow from this role.
  All three application variants and the explicit installed driver build. All
  202 CTests (including architecture) and 436 installed-helper Python tests pass.
  The pure status codec/ledger/text suite also passes AddressSanitizer and
  UndefinedBehaviorSanitizer. The matching installed lifecycle qualification
  below passed; this does not complete campaign-time controls or co-op.
- Corrected installed run `_rzq34v5` **passed** creation, private-credential
  reconnect, clean checkpoint/cold resume, A/B movement, clock/leader agreement,
  leader disconnect/reconnect, inventory/controller and shipping Screen swaps,
  worn-equipment restoration, the Screen drop/pickup sequence, portrait capture
  and tactical/campaign stale-state clearing after authority shutdown. Client B
  was the leader: both clients agreed at campaign revision 5 / 111600 seconds,
  A observed B offline at revision 8 / 111602 seconds without taking over, and
  both agreed again at revision 43 / 111634 seconds with B still leader and
  leadership revision unchanged. After shutdown each `CAMPAIGN` response exposed
  only request/kind/`ready:false`, with no retained clock or role fields. The
  consolidated report was successfully saved this time. No test time-control
  commands, setup gear/wounds or explicit interrupt passes/EndTurn were used.
  Three final PNGs were personally inspected: both native views show day 1
  07:00:34 and their correct local leader labels; the native pause flag later
  reads `on`, real portraits and the red cursor are visible, and the selected
  Locksmith Kit shows 99/99. This native pause observation is **not** evidence
  for pause-on-leader-loss. Travel/strategic controls, installed first aid,
  firing/reload and full turn-cycle qualification remain separate.
  All owned processes exited and protected installed inputs remained unchanged.
  Artifacts:
  `/var/folders/9y/v1xx2nbd5jg8pt7ntqtndr0w0000gn/T/ja2-coop-installed-smoke-_rzq34v5`.
- Installed campaign-status run `7focegwt` reached and passed creation, credential
  reconnect, checkpoint resume, two-client clock/leader agreement, A/B movement,
  leader disconnect without takeover, same-credential leader reconnect,
  inventory/Screen swaps, worn-equipment restoration, Screen drop/pickup,
  portraits and stale tactical/campaign clearing after server shutdown. Its
  overall result was **failure**, however: the final evidence writer did not
  recognize the new `campaign-status` report kind, so no consolidated campaign
  JSON was saved. This was a test-runner integration error, not a runtime
  failure. The writer now recognizes that exact kind and requires explicit
  non-claims for time controls/offline pause; two new regression tests cover
  saving it without overwriting and rejecting inflated coverage. The corrected
  rerun above passed. All owned processes were cleaned up and installed inputs remained
  unchanged. Three captured PNGs were personally inspected: both native views
  display day 1 07:00:34, client B says `Time leader: you`, client A says
  `another player`, real portraits and the red cursor are visible, and the
  inventory readout correctly shows Locksmith Kit 99/99. These images do not
  certify actual time-control input. Artifacts:
  `/var/folders/9y/v1xx2nbd5jg8pt7ntqtndr0w0000gn/T/ja2-coop-installed-smoke-7focegwt`.
- The ordinary-equipment extension builds all three application variants and
  the explicit installed driver. All 200 CTests and 423 installed-helper Python
  tests pass, including the corrected inspection checks described below. Ten
  native swap/transfer/drop/pickup cases each passed ten repeated runs, including
  armor class/position/stack validation, face compatibility/counterpart changes,
  Nails' vest policy, camouflage refresh and required-light failure containment.
  These fixtures do not certify installed face equipment or painted gear effects.
- The corrected matching installed run `s5vd6oal` **passed** the complete
  requested smoke: campaign creation, Ready, private-credential reconnect,
  checkpoint resume, two-client movement and moved-state recovery, controller
  inventory and two shipping Screen swaps, worn-armor restoration, same-owner
  transfer/return, controller drop/pickup, the twelve-step Screen drop sequence,
  loot-model correlation, portrait capture and stale-state clearing after server
  shutdown. No setup objects/loadouts were injected, no explicit interrupt passes
  or EndTurn were needed, all owned processes exited, and installed input
  metadata remained unchanged. Artifacts are retained under
  `/var/folders/9y/v1xx2nbd5jg8pt7ntqtndr0w0000gn/T/ja2-coop-installed-smoke-s5vd6oal`.
  Client B's Vinny `1:2` used commands 9..12 to stow his real Locksmith Kit
  6→21, remove vest item 188 into offhand 1→6, restore it 6→1 and restore the
  toolkit 21→6. Private revisions advanced 8→12, both public armor/hand
  projections agreed throughout, and every original slot was restored. AP
  remained 96; this is agreement, not inventory AP-cost certification.
  Client A's Kaboom `0:1` dropped/recovered his real pocket-35 item 924
  (six total rounds) first through the controller, then through shipping Screen
  input. The latter required twelve queued steps, rejected held-key confirmation,
  cancelled safely with Escape and used exactly commands 4/5 to create then
  retire ground identity `7:9`, restoring the original pocket. Combat AP
  76→64→52 verifies 12 AP per drop/pickup. This is real Screen queue coverage,
  not physical OS input, painted loot markers or full hidden objects. All three
  PNGs were personally inspected: intact toolkit 99/99 readout, real
  Vinny/Bull and Kaboom/Gary portraits, and visible red cursor. Installed face
  gear, first aid, firing/reload and full turn-cycle acceptance were not requested
  and gain no qualification from this run.
- Installed equipment run `0bxdziuo` passed creation, private-credential
  reconnect, checkpoint resume, two-client renderer readiness, A/B movement and
  moved-state recovery after client restart. Movement was real-time, not combat
  AP-cost certification. Inventory readiness required one explicit interrupt
  pass and no EndTurn; four controller swaps and two Screen swaps restored the
  original owner summaries. The complete run then **failed** in its old
  inspection-only test: both its helper and driver still treated ordinary vest
  slot 1 as read-only. Clicking it with the tool selected unintentionally
  submitted command 9, which the native authority rejected (reason 12), and the
  later source-toggle expectation timed out. This is a test-path defect, not a
  successful equipment test. Equipment removal/restoration, transfer, drop,
  new drop-input, portraits and final disconnect checks were not reached.
  No runtime/replication failure was logged; test processes were cleaned up and
  installed input metadata remained unchanged. Retained artifacts:
  `/var/folders/9y/v1xx2nbd5jg8pt7ntqtndr0w0000gn/T/ja2-coop-installed-smoke-0bxdziuo`.
  The corrected helper and rebuilt driver now share the shipping 0..6/14..54
  swap-location policy for inspection safety, while the existing two-swap UI
  test remains hand/pocket-only. Inspection selects an actually unsupported
  worn-LBE/opaque item, and any unexpected command/receipt change aborts before
  another input. Regression tests cover ordinary armor not being read-only,
  old-inventory visibility and command-side-effect rejection.
- Ordinary whole-stack pocket drop is implemented and narrowly qualified: intent kind
  14, retained native journal tag 39, detached allocation before publication,
  and an `X` → release → `X` confirmation in the existing inventory panel.
  All three matching application variants and the installed driver compile.
  Both native drop fixtures pass, and all four native drop/pickup fixtures pass
  ten repeated runs each. All 196 CTests pass (195 regression cases plus the
  architecture suite), as do 354 installed-helper Python tests. The matching
  installed run below passed, but this is not full co-op or release completion.
  Protocol and journal versions remain unchanged.
- The matching installed `ffmlbafa` run passed creation, Ready, private-credential
  reconnect, clean checkpoint/cold resume, renderer-ready two-client A/B movement,
  moved-state preservation after B restarted, ordinary inventory controller and
  actual Screen equip/restore, same-owner transfer/return, drop/pickup, and
  shutdown-to-reconnecting cleanup without stale public state. No objects or
  loadouts were injected, and installed input metadata remained unchanged.
  Evidence is retained under
  `/var/folders/9y/v1xx2nbd5jg8pt7ntqtndr0w0000gn/T/ja2-coop-installed-smoke-ffmlbafa`.
  In `drop-evidence.json`, client A's Vinny `1:2` dropped pocket 25's existing
  item 77 stack (two magazines, 12 total rounds) at grid 4711 with command 11,
  then recovered exact ground identity `7:8` into the original pocket with
  command 12. Owner revisions advanced 10→11→12 and combat AP 92→80→68:
  exactly 12 native AP for each action, with matching public state and all
  original carried/ground/nearby summaries restored. The between-action copied
  loot model positively exposed that one authorized nonmedical marker at
  `(723,134)`; after pickup both selected clients' marker models were empty.
  This certifies controller execution and exposed summary/identity/model
  conservation, not physical X/P input, painted loot pixels or installed opaque
  object graphs. All three retained PNGs were visually inspected: the inventory
  kit readout, Vinny/Bull and Kaboom/Gary rosters, and red cursor render. Firing,
  reload, first aid and a full turn cycle were not requested in this run and
  gain no new qualification from it.
- The nearby-loot build compiles all three application variants and the
  explicitly rebuilt installed driver. All 193 CTests and 333 installed-helper
  Python tests pass. The two native pickup fixtures each pass ten repeated
  runs, now including nearby candidate/projection checks: radius and floor
  boundaries, no row wrap, persistent individual visibility, debug-show-all
  isolation, hidden ownership suppression, medical-point classification,
  nearest-128 selection across 169 real tile pools and unchanged revisions on
  repeated capture. Codec/session/client tests reject malformed, noncanonical
  or out-of-bounds markers and prove that a location hint cannot authorize
  pickup. UI tests cover static terrain/roof/render-height composition,
  clipping, hover raster, stale context, modal/actor priority and one-frame
  overlay clearing. The read-only installed verifier distinguishes coherent
  empty/offscreen views from positive marker-model observations; those unit
  tests do not certify installed marker paint, mouse input or pickup.
  No protocol versions were incremented.
- Installed run `ja2-coop-installed-smoke-tvrlp4sq` passed campaign creation,
  credential-preserving reconnect, checkpoint resume, both clients' movement
  and moved-state preservation after tactical reconnect. Movement was in real
  time, so this proves replica/AP agreement, not combat movement cost. On client
  A, Vinny's four controller swaps and two actual Screen swaps restored his
  inventory (private revision 2→8). Commands 9 and 10 gave his ammunition pocket
  25→Bull's pocket 26 and returned it, with private revisions 8/1→9/2 and
  2/9→3/10 and exactly 2 combat AP charged to each merc in both directions.
  One explicit interrupt pass preceded inventory readiness; no EndTurn was
  submitted. All three screenshots were personally inspected: Vinny/Bull and
  Kaboom/Gary stock portraits, Locksmith Kit tool points 99/99, intact inventory
  labels and visible red software pointer. The read-only loot correlation
  passed on its first attempt for both current owner scopes; both lists were
  empty. This is not installed marker paint, marker-click or pickup evidence.

  The complete smoke did **not** pass: its later fire stage exhausted its
  90-second deadline without an eligible public hostile target or any FIRE
  submission. The reported state-request timeout was misleading: both client
  logs contain that exact request 626 at revision 5838, with zero hostiles and
  advancing earlier revisions. No runtime/replication failure or attack-busy
  underflow was logged. Fire, reload, the final shared turn-cycle check and
  post-shutdown stale-state check were not completed on this build. Test-owned
  processes exited during cleanup and installed input metadata remained
  unchanged. Earlier combat evidence below is historical, not a substitute for
  this incomplete matching-build run.
  The fire probe now preserves the original failure plus deadline status,
  last public-target/candidate counts and submitted-command observations.
  Six additional regression tests cover deadline crossing and genuine probe
  failures without loosening acceptance or retrying gameplay. This diagnostic
  update was unit-tested after the run; the installed failure above is retained.
- The final ground-pickup build compiles all three application variants and
  the explicitly rebuilt installed driver. All 191 CTests (including the
  architecture and tile-access guards) and 313 installed-helper Python tests
  pass. Ground-pickup native fixtures pass ten consecutive runs each, including
  exact whole-stack medical-point/list-node conservation, native combat and
  real-time AP behavior, unchanged AP on rejection, competing/duplicate
  requests, retired/reused item incarnations and removal-callback failure
  containment. Real 66/67-item pools verify the 64-entry cap, sorted dense
  summaries, an ownership marker at the tail excluding the whole pool, and
  unchanged revisions on repeated capture. Mirror divergence fails without
  identity repair or changed caller output. Codec/session/host/client/UI tests
  cover exact sent-item/revision admission, malformed frames, atomic private
  replacement before Applied, context cancellation, modal isolation and panel
  geometry. No protocol/journal versions were incremented.
- Installed run `ja2-coop-installed-smoke-8i21hcir` passed campaign creation,
  credential-preserving reconnect, clean checkpoint resume, both clients'
  movement and moved-state preservation after tactical reconnect. Vinny's four
  controller swaps and two actual Screen click swaps restored his original
  inventory, advancing private revision 2→8 (movement had already advanced it).
  Command 9 gave Vinny's ammunition pocket 25→Bull's empty pocket 26, advancing
  revisions 8/1→9/2; command 10 returned it, advancing Bull/Vinny 2/9→3/10.
  Both directions charged exactly 2 native combat AP to each merc, with exact
  summary restoration and unrelated owner inventories unchanged. Fire consumed
  one round with replica/AP agreement, not combat-cost certification. Reload
  added one round with native combat-cost verification and exposed ordinary
  reserves decreasing 6→5; the shared turn cycle advanced 1→4 with two explicit
  interrupt passes. All three screenshots were visually inspected: stock
  Kaboom/Gary and Vinny/Bull portraits, Vinny's Locksmith Kit tool points 99/99,
  unclipped inventory text and the visible red software pointer. All test-owned
  processes exited and installed input metadata stayed unchanged.
  The 412 ready-state records contained 824 owner inventory observations with
  ground context present, but every observed ground list was empty. This run
  does not prove installed pickup, the P/mouse path, kit acquisition or treatment.
  It preceded only an equivalent checked tile-accessor cleanup and additional
  native fixture assertions; the final application/driver rebuild is separate.
- The same-owner transfer build compiles all three application variants and the
  explicitly rebuilt installed driver. All 189 CTests (including the architecture
  guard) and 313 installed-helper Python tests pass. Both isolated native transfer
  tests pass ten repeated runs. Coverage includes exact whole-stack kit/ammo
  conservation, both actors' AP, stale objects/identities/poses, LOS and native
  pocket/LBE restrictions, unchanged opaque offhands in pocket-only transfers,
  real hand-equipment/palette roundtrips, immediate recipient directory refresh,
  and post-mutation animation failure containment. Network tests cover both
  owners/revision tokens, duplicate receipts, both private replacements preceding
  Applied, and deferred ownership changes with a bounded watchdog that sends no
  stale state or receipts. Client tests cover source/recipient selection,
  cancellation, state invalidation, panel rendering and blocked terrain input.
  These fixture results do not certify cross-player trading, ground pickup or
  the installed transfer panel's mouse-input path.
- Installed run `ja2-coop-installed-smoke-77nvyo83` passed campaign create,
  credential-preserving reconnect, checkpoint resume, both clients' movement
  and moved-state recovery after tactical reconnect. Client B's Vinny completed
  four controller swaps and two actual Screen click swaps, restoring the original
  summaries and advancing private revision 1→7. The new controller transfer gave
  Vinny's whole ammunition stack from pocket 25 to Bull's empty pocket 26, then
  returned it exactly: command 9 advanced their revisions 7/1→8/2, and command 10
  advanced Bull/Vinny 2/8→3/9. Each direction charged exactly 2 native combat AP
  to each merc. Both complete owner summaries and public hands were restored;
  unrelated owner inventories stayed unchanged. The test made exactly two
  transfer submissions and needed no readiness interrupt pass or EndTurn.
  Both transfer checks used the real controller, not the Screen's G/click path.
  The retained records also passed the helper's two final hardening checks:
  all 393 ready-state observations had complete owner caches; command 9's six
  and command 10's 79 Applied observations each retained one identical receipt.
  All three PNGs were visually inspected: Kaboom/Gary on client A, Vinny/Bull on
  reconnected client B, and Vinny's Locksmith Kit details (tool points 99/99).
  Portraits, borders, labels and the red software pointer remain visible and
  unclipped. No transfer-mode screenshot was captured. Fire consumed one round
  with replica AP agreement (not combat-cost certification); reload added one
  round with combat-cost and exposed ordinary reserve-conservation checks.
  One shared EndTurn returned ordinary player turn 1→4 with two interrupt passes.
  Test-owned processes exited, disconnect cleared stale client state, and the
  installed input metadata remained unchanged. This is not ground pickup,
  installed first aid, cross-player trading or full campaign qualification.
- The global-v13 medical-service build compiles all three application variants
  and the installed driver. All 187 CTests, including the architecture guard, and 300 build-free
  installed-helper Python tests pass. The native first-aid fixture and all three
  isolated inventory fixtures each pass ten repeated runs. Native medical tests
  cover real startup/continuation, provider and healthy-recipient Stop, truthful
  critical-recipient rejection, self-service representation and unresolved/
  private/retired patient capture. Wire/replica/HUD tests cover service-only
  changes, canonical rejection, zero bleeding without inferred completion,
  exact-incarnation replacement, UTF-16 patient names and bounded text. Service
  changes are gameplay-significant even when an animation frame also advances.
  Native inventory roundtrips now verify equipment-derived camouflage in both
  hands, native caps, unchanged applied camouflage and exact AP/object/journal
  behavior. A genuine palette-refresh failure after mutation invalidates the
  world, charges no AP and cannot replay the swap. These are fixture/regression
  results, not installed treatment, kit acquisition or full inventory completion.
- The matching global-v13 installed run `ja2-coop-installed-smoke-dxc2zizx`
  passed campaign create/reconnect/checkpoint resume, movement from both clients
  and preservation of moved state after tactical reconnect. Vinny on client A
  completed four controller swaps and two real Screen click swaps, restoring the
  original typed summaries/public primary and advancing private revisions 1→7.
  Inventory readiness needed no interrupt pass or EndTurn. All three retained
  PNGs were visually inspected: Vinny/Bull on client A, Kaboom/Gary on reconnected
  client B, and Vinny's Locksmith Kit inventory details (`Tool points`, first/total
  99). Portraits, selection borders and text remain unclipped; the red software
  cursor is visible. Fire and reload each changed loaded ammunition by one round
  with native combat AP checks. The reload's exposed ordinary reserve rounds
  decreased 24→23, private revision 2→3, in one attempt. One shared EndTurn
  returned the ordinary player turn 4→7 with two explicit interrupt passes;
  the convergence check now also validates medical-service descriptors. All
  test-owned processes exited, clients cleared stale state after disconnect,
  and installed-content metadata was unchanged. This is matching-build
  integration evidence, not an installed first-aid action, camouflage-bonus
  measurement or full inventory/campaign completion.
- The initial global-v13 installed attempt `ja2-coop-installed-smoke-b9s332we`
  completed normal-client reconnect and checkpoint resume, but its separately
  excluded installed-driver target had not been rebuilt with the normal apps.
  Both tactical drivers were rejected at bootstrap; no gameplay was certified.
  The driver was then explicitly rebuilt before the matching-binary rerun.
- The global-v12 static-portrait build compiles all three application variants
  and the installed driver. All 187 CTests, including the architecture guard,
  and 294 build-free installed-helper Python tests pass. Native capture and pure cache fixtures
  each pass ten repeated runs. Native tests cover profile/face differences,
  IMP faces, 151–154 aliases, absent/out-of-range profiles, applied-camouflage
  priority and unchanged simulation RNG/legacy face bindings. The shipping
  native blitter passes exact face-only writes and restrictive/empty/inverted
  clipping restoration, plus fourteen malformed asset cases rejected before
  writes. Wire/delta/replica tests cover descriptor-only changes, canonical
  rejection and replacement; cache tests cover six-slot limits, revocation,
  missing assets and same-actor camouflage fallback. Installed screenshots
  remain a separate acceptance check, not something those fixtures certify.
- The matching global-v12 installed run `ja2-coop-installed-smoke-5uie3bbh`
  passed campaign create/reconnect/checkpoint resume, movement from both clients
  and moved-state preservation after tactical reconnect. Both native roster PNGs
  were subsequently visually inspected: client A shows Kaboom and Gary, and
  reconnected client B shows Vinny and Bull, with actual stock portraits inside
  the face rectangles, intact names/meters/selection borders and visible red
  software cursors. `portraits-evidence.json` records authoritative descriptor
  agreement and capture scope; the automated probe does not claim visual QA.
  Installed IMP/camouflage variants remain unverified beyond fixtures. Four
  controller swaps and two real Screen click swaps restored the original typed
  inventory summaries/public primary loadout, advancing private revisions 1→7;
  inventory readiness needed no interrupt pass or EndTurn. Fire consumed one
  round on both replicas (AP agreement only, not combat AP certification).
  Reload added one loaded round with native combat AP cost and exact exposed
  ordinary reserve rounds 6→5, private revision 2→3, in one attempt. This is
  aggregate ordinary-round evidence, not hidden LBE/full-object or per-ammunition-
  type conservation. One shared EndTurn advanced the ordinary player turn 1→4
  with three explicit interrupt passes. All test-owned processes exited,
  disconnected clients cleared stale state, and installed-content metadata was
  unchanged. Actual medical treatment and a complete campaign remain unverified.
- The global-v11 typed-inventory build compiles all three application variants
  and the installed driver. All 184 CTests, including the architecture guard,
  and 277 build-free installed-helper Python tests pass. Ordinary ammunition, medical kits and tool
  kits now expose exact whole-stack totals and distinct units; other ordinary
  items show first-object condition. Native tests cover non-first-object changes,
  depleted first kits with later points, unsigned ammunition above 32,767,
  the 255-object maximum, transitions to opaque attachment state and semantic-only
  revision changes. All three isolated inventory fixtures also pass ten repeats.
  Network tests reject conflicting metrics at the same private revision and
  replace them at a new revision while retaining owner/baseline/receipt barriers.
  This is bounded summary coverage, not complete inventory replication.
- The matching global-v11 installed run `ja2-coop-installed-smoke-pydoxj8j`
  passed campaign create/reconnect/checkpoint resume, movement from both clients
  and moved-state preservation after tactical reconnect. Four controller swaps
  and two real Screen click swaps restored all original typed summaries and the
  public primary loadout; private revisions advanced 1→7. The inspected native
  PNG shows Vinny's Locksmith Kit as `Tool points`, `First 99`, `Total 99`, with
  the red software cursor visible and the sidebar text unclipped. Its eight-input
  probe also checked that inspecting an unsupported equipment location preserved
  the swap source, then cancelled that source before capture. `inventory-evidence.json`
  records these separate actions and observations. Fire and reload each changed
  loaded ammunition by one round with native combat AP checks. The optional
  combined inventory/reload certificate observed ordinary reserve rounds 24→23
  for actor 2:3, private revision 2→3, exactly matching the one loaded round added
  in a single reload attempt. `reload-reserve-evidence.json` retains the original
  and resulting summaries. This proves aggregate exposed ordinary rounds, not
  hidden LBE, complete objects or per-ammunition-type conservation. One shared
  EndTurn advanced the ordinary player turn 1→4 with one explicit interrupt pass;
  inventory readiness needed no pass or EndTurn. All test-owned processes exited,
  disconnected clients cleared stale state, and installed-content metadata was
  unchanged. Actual medical treatment remains unverified in installed play.
- The passive-client missing pointer is fixed: both loading/reconnect and game
  frames restore the native software cursor after rendering, without invoking
  legacy tactical hover/aim hooks or inheriting a roof offset. The rebuilt three
  application variants and installed driver pass all 184 CTests (including the
  architecture scan). Native screenshot regressions verify visible pixels at
  the mouse position, recovery after hiding, inherited-clip restoration and no
  framebuffer trails. The subsequent installed run `ja2-coop-installed-smoke-oick7hka`
  also produced a visually inspected inventory screenshot with the red game
  pointer visible. A manual two-window focus check remains unperformed.
- The global-v10 cursor/inventory-click build compiles all three application variants
  and the installed co-op driver. All 183 non-architecture CTests and 258
  build-free installed-smoke Python tests pass. The full architecture scan also
  passes, including ACK progress and one-message-budget private-publication
  barriers. These are regression results, not proof of the installed equipping
  acceptance below.
- The native inventory default, post-mutation-failure and missing-animation
  fixtures each passed ten repeated runs. The default fixture now additionally
  verifies occupied loaded-gun/kit and two-ammunition-object pocket roundtrips,
  preserving complete ordinary-state fingerprints, a single retained command
  and native zero-AP costs for same-type locations. Those extra item definitions
  and animation surfaces are explicitly synthetic, not installed-content proof.
- The global-v10 installed run `ja2-coop-installed-smoke-nx773iq8` passed
  campaign creation, client reconnect, cold checkpoint resume, movement from
  both clients and preservation of moved actor state after reconnect. It then
  failed inventory prerequisites without submitting a swap: a native interrupt
  allowed actor 2:3 to act, while actor 1:2 carried the suitable ordinary kit.
  No inventory screenshot, equipping outcome or later combat action was certified
  by that run. The probe must advance that interrupt through the existing exact
  eligible-actor pass intent before requiring an ordinary-turn inventory action;
  bypassing native eligibility is not a fix.
- With that readiness fix, `ja2-coop-installed-smoke-oick7hka` passed the full
  installed lifecycle and movement/reconnect checks, four exact interrupt passes
  without EndTurn during readiness, ordinary gun/locksmith-kit equip/restore,
  two-object reserve-stack pocket/restore, and `I`/inspect/screenshot/`Esc` through
  the shipping inventory screen. The owner inventory revision advanced 1→5;
  both public primary projections agreed and original summaries were restored.
  Native fire consumed one round (AP agreement only), reload restored one round
  with combat AP cost, and one shared EndTurn returned player turn 4→7 with two
  further interrupt passes. The cursor is visible in the inspected native PNG.
  This run predates the extra click-driven equip/restore probe: its four swaps
  used the shipping controller through the test driver, not mouse clicks.
- The subsequent matching-build run `ja2-coop-installed-smoke-silm340q` passed
  the full lifecycle again, including both ordinary source/destination-click
  swaps on the actual Screen-selected Vinny. UI commands 7 and 8 advanced the
  private revision 5→6→7 and restored the original summaries/public primary.
  The inspected native PNG shows the red pointer and the unclipped `Inspect only`
  warning. `inventory-evidence.json` retains separate readiness, four controller
  actions, two UI actions and screenshot evidence. This run needed no readiness
  or turn-cycle interrupt passes; the previous run supplies that separate coverage.
  Fire consumed one round, reload restored one, both with combat AP checks;
  the ordinary player turn advanced 1→4 after one shared EndTurn. All test-owned
  processes exited and installed-content metadata remained unchanged. This
  validates bounded equipping, not full inventory graphs or installed first aid.
- The 2026-09-05 validation finished with all three application variants and
  the installed co-op driver building successfully, all 178 registered CTests
  passing (177 regression tests and the architecture guard), and 184 build-free
  installed-smoke Python tests passing. Each of the three isolated first-aid
  failure/standing-start cases also passed ten repeated runs.
  These are regression results, not completion of the acceptance items below.
- The installed two-client lifecycle `ja2-coop-installed-smoke-s912um1h`
  exercised movement from both clients, live client reconnect, aimed fire,
  reload, shared-team EndTurn, an explicit serial-bound interrupt pass, return
  to an ordinary player turn, and cleanup/reconnection after server loss.
  These were **pre-medical binaries**. This is bounded native turn-cycle and
  replica-convergence evidence, not a complete firefight or campaign soak.
  See [installed evidence](MULTIPLAYER_ARCHITECTURE.md#installed-turn-cycle-evidence)
  and the [installed lifecycle driver](../tests/full_engine_coop_installed_process_smoke.py).
- The matching global-v9 medical-state build subsequently passed the same
  lifecycle in `ja2-coop-installed-smoke-uh2nikua`: combat-cost fire and reload,
  player turn 4 to 7, four explicit interrupt passes, and strict two-client
  bleeding/held-kit-state agreement. This validates the new wire integration,
  not an installed first-aid action: no kit was equipped or treatment requested.
- First-aid initiation now has wire, authority and native regression coverage.
  It requires a usable kit already equipped and a distinct adjacent wounded
  teammate. Applied means treatment started, not treatment completed.
  Native continuation is also covered: the data-free fixture supplies the normal
  animation-completion marker, then runs real team services to prove reduced
  bleeding, consumed kit points, treatment AP cost and authoritative Stop
  cancellation. This is not an installed animation/treatment proof. The game
  already calls services at turn end and during real-time turns. Installed/manual
  first aid and a medical-kit acquisition/equipping flow remain unproved; the
  installed equipped locksmith kit is a tool kit, not a medical kit.
  Entry points: [intent vocabulary](../Multiplayer/CoopTacticalIntent.h),
  [native command execution](<../Tactical/Simulation Commands.cpp>), and
  [fresh-process native fixtures](../tests/ja2_headless_tests.cpp).
  The isolated [startup-failure fixtures](../tests/authoritative_first_aid_failure_tests.cpp)
  additionally pass real post-AP/service allocation failure, ignored native
  animation-load failure, and successful standing-to-kneeling initiation with
  pending `START_AID`. Partial failures invalidate the world, cannot produce
  Applied, and cannot repeat treatment on a forced later drain. They verify the
  native queue and observer-source boundary, not an installed network failure
  or completed bandaging.
- `EstablishedStrategicCold` accepts a bounded, validated living on-foot
  wounded/travelling roster and explicitly stops compression and pauses native
  time. Direct native inspection and policy tests cover classification and
  non-mutation; they do **not** prove a travelling/wounded disk save/load cycle.
  Existing accepted stationary mixed rosters retain their prior launch policy.
  Entry points: [classifier](../Ja2/DedicatedCoopMissionBootstrap.cpp),
  [policy](../Ja2/DedicatedCoopMissionPolicy.h), and
  [runtime lifecycle](../Ja2/DedicatedCoopRuntime.cpp).

## Ordered completion acceptance

### 1. Owner-only inventory and usable mercenary panels

- Replicate the complete authorized inventory to its controlling owner,
  including reserve ammunition, stacks, attachments and LBE; do not leak it
  through a public actor snapshot. Keep observer equipment projections bounded.
- Inspect, equip, swap and transfer through validated server intents with exact
  actor/item identity, revision checks, truthful rejection and no ambiguous replay.
- Show usable loadouts, item names and actual manifest-bound portraits instead
  of placeholder faces. Both clients must see appropriate public equipment
  changes; reconnect must replace stale private and public inventory state.
- Acceptance: a player finds and equips a carried kit or weapon using the game
  interface, without a console, fixture injection or client-local mutation.

Start at [snapshot state](../Engine/Adapters/JA2/TacticalWorldSnapshot.h),
[authority](../Multiplayer/CoopTacticalIntent.cpp),
[native inventory gateways](../Ja2/TacticalInventoryUiHost.cpp), and
[passive presentation](../Ja2/FullEngineCoopClientPresentationPassiveUi.cpp).

The first bounded slice is implemented for validation: press `I` in the passive
game view to inspect the selected owned merc's carried slot summaries. Click an
ordinary item in a helmet, vest, leggings, face, hand or pocket slot, then another
supported slot to request a whole-stack swap; click the selected source again
to cancel it, or `Esc`/`I` to
close. Item names and graphics come from the verified local content, while slot
contents and the independent inventory revision come from the server. Orange
`!` slots are inspect-only in this slice. Placement, pocket capacity, backpack
access, two-handed restrictions, AP, idle state and object proofs are checked
again by the native authority. Armor must match its native equipment position;
face items must remain compatible with the other equipped face item. The native
Nails leather-jacket restriction is retained without opening a server dialogue.
There is no local item cursor or speculative swap.

Same-owner transfers now use the same panel: select an ordinary hand/pocket
item, press `G`, cycle nearby owned recipients with `Tab`/`[`/`]`, and click an
empty supported destination. The heading shows giver, recipient and stack.
`Esc`/`G` cancels; neither client's inventory is modified speculatively. Changes
to either private revision, ownership, baseline, world or eligibility cancel the
selection. During an active interrupt the client offers it only when both mercs
are interrupt-eligible (each spends AP), matching the authority; never while an
interrupt is resolving. Ordinary pocket drop follows the same rule for the
selected merc.

The retained `TransferInventoryItemCommand` follows stock inventory-panel rules:
two conscious human mercs, native distance/LOS and pocket capacity checks, a
3-AP combat admission gate and 2 AP charged to each after placement. Both exact
actors and object/pose proofs are revalidated at execution. Commands involving
hands retain checked equipment/palette continuations. Pocket-to-pocket transfers
leave hands untouched, including unsupported offhand objects, and skip those
irrelevant continuations. Any post-mutation failure invalidates world integrity
instead of returning an ordinary rejection that could be retried.

Both private inventory revisions must have been sent to the same owner before
admission; all pending owner inventory replacements precede terminal receipts.
Actor reassignments defer while retained native commands are pending, preserving
the ownership under which they were admitted. Exact unchanged assignments stay
idempotent. This is not cross-player consent or a general-purpose trade system.

Basic ground pickup now uses the same owner-private inventory stream. The server
lists at most 64 visible, neutral, ordinary items on the merc's exact tile/floor,
with exact native item incarnations and typed whole-stack summaries. Tile,
item-state or list changes advance the private inventory revision. A hidden
OWNERSHIP marker excludes the entire pool; hidden/trapped/armed/complex objects
are not offered. Read-only capture never discovers items or repairs identities.

`I` → `P` browses that list, `Tab`/`[`/`]` chooses an item and an empty pocket click
requests one whole-stack pickup. `Esc`/`P` cancels; `I` closes. Busy mercs can
inspect the list but cannot submit. Stale pose, owner, assignment, revision or
baseline cancels the selection. One retained native command rechecks identity,
actor/pool/object state, slot capacity and AP; it moves existing object storage,
removes the native pool entry/graphic, retires the exact item incarnation and
charges the native pickup cost once. A competing/duplicate request cannot take
the same item again. Post-mutation failure invalidates world integrity instead
of advertising success or replaying the move.

Nearby discovered ordinary piles now have bounded location-only markers in the
normal game view: same floor, within 12 tiles, nearest 128 tile locations. A
cross identifies positive ordinary medical supplies, not treatment readiness.
Click a marker to request normal movement, then use `I`/`P` or click the marker
under the selected merc to browse the exact pickup list. The view neither
discovers hidden items nor offers owned pools, and the location hint itself
cannot authorize pickup. Copied owner/revision/pose/camera context gates clicks;
the static renderer clears old overlays when selection or loot changes.

This is not full loot handling: automatic walking-and-pickup chains,
hidden-item discovery, theft/ownership reactions, disarming, complex objects,
merging, splitting, broader drop placement and installed medical-supply acquisition still need
implementation or qualification. The driver can observe the authoritative
ground list and submit the controller-only `PICKUP` operation; neither is proof
that an installed pickup or the shipping P/mouse flow has been exercised.

Ordinary whole-stack pocket drop is now implemented as another bounded native
command. In `I`, select a supported ordinary pocket stack (slots 14–54), press
`X`, release it, then press `X` again to confirm. The modal identifies the item
and count; `Esc` cancels and `I` closes without moving anything. Held-key repeats
cannot confirm. Hands/equipment, splitting and complex objects are excluded.
Changed private revision, owner, assignments, session/world/baseline, actor pose
or readiness cancels the confirmation. Clicking the world or another inventory
slot while confirming cannot move the merc or drop the item.

The server alone decides placement: the merc's exact dry ground-level
(`FIRST_LEVEL`) tile must have no item pool, live world item, mine marker,
roof/overhead rendering node or unsupported structure. There is no relocation,
container placement, ownership/trap conversion or client-chosen destination.
The checked insertion path prepares detached world-item storage, graphic/pool
nodes and identity capacity before rechecking source and placement. It then
publishes one fresh identity and links the complete pool/graphic without further
allocation, preserving exact object storage, including unrelated world objects
during vector growth. Success requires a canonical-empty source and the exact
new ordinary ground object before charging the native basic pickup AP cost once
(no AP charge in real time). Ordinary pre-mutation rejection leaves items/AP
unchanged; an integrity failure after publication poisons the world rather than
reporting an ordinary retryable rejection or Applied. Native fixtures cover
allocation-failure sweeps through successful nonempty-vector growth, exact
unrelated action-item/attachment storage preservation, retired-slot reuse,
identity exhaustion, stale/duplicate rejection, ambidextrous AP and backpack
access. Direct `MemAlloc` failure injection remains untested; cleanup of both
detached native nodes after a later C++ allocation failure is covered. The
matching installed drop→pickup evidence above is positive; physical X/P input,
broader placement and installed medical-supply acquisition remain unqualified.

The additional `--verify-drop-input` installed probe queues a fixed twelve-step
sequence through the shipping Screen: open/select, arm, held repeat, Escape
cancel, reselect/arm/release/confirm, open pickup, recover into the original
pocket and close. It requires no gameplay before confirmation, exactly two
Applied commands and exact carried/ground restoration. This is queued Screen
input coverage, not physical OS mouse/keyboard or painted-marker verification.
`--verify-equipment` separately exercises controller-backed worn-armor removal
and restoration, including explicit offhand stowing when needed; it does not
certify physical armor-slot clicks or face-equipment graphics.

Automatic walking/pickup is separate: it needs a cancellable
server-owned continuation and fresh exact-item checks on arrival, not an
automatic follow-up inferred from a location-only marker.

The static portrait slice is implemented and installed-verified for stock faces:
authoritative normal/IMP face identity and applied camouflage feed a bounded six-slot static-image cache in
the stock TEAM roster. Missing/corrupt art stays neutral; there is no cold client
profile lookup, random face initialization or local tactical simulation. Animated
faces, speech and facial equipment overlays are not part of this slice.

This is not complete inventory replication: summaries include item/count/first
native status, typed whole-stack ammo/medical/tool totals and an explicit
unsupported-complexity marker, not complete stack objects, attachments or active
LBE graphs. The panel labels those resource kinds separately from condition;
opaque objects retain unknown semantics and no invented total. Ordinary item classes currently cover
guns, ammunition, blades/throwing knives, armor, medical/tool kits, face items
and plain miscellaneous objects with no traps, attachments or alternate union
payload. Miscellaneous first-status values retain an unknown metric; moving a
canteen does not implement drinking or prove its water resources. World-control
markers are excluded regardless of their item class;
ordinary armor and face slots now participate in same-actor swaps alongside
hands and pockets. Equipped LBE locations, cross-player transfers,
occupied-destination exchanges between mercs, splitting,
merging and automatic stowing remain unsupported. The
native first-status union is ammunition quantity for ammo, not always condition.
Private object-state fingerprints detect observed ordinary changes; they are not
persistent item identities or proof against every unobserved remove/reinsert cycle.

The command is a single retained `SwapInventorySlotsCommand` (journal tag 36).
Both destinations, object state, AP and equipment continuations are validated
before success. Existing quick-item/UI helpers may move or stow items before
reporting failure, so they are not used as network validators.

The retained swap now also refreshes native equipment-derived camouflage, as the
stock inventory path does. Held weapons can grant those bonuses even in the
existing hand/pocket slice. Face-slot changes also preflight and refresh native
personal lighting through a checked continuation; daylight or disabled personal
lighting is a legitimate no-op, while failed required light creation is not.
Face-pair proofs include the unchanged opposite slot, and all swaps still require
ordinary public-hand object proofs: an opaque offhand can therefore block an
otherwise ordinary armor swap. Palette or required-light failure after mutation
invalidates world integrity rather than returning Applied or allowing a repeated
swap. These extensions reuse the existing intent and journal tag without version
increments; transfer and drop retain their narrower location restrictions.

The retained command design must not recursively dispatch equipment commands:
the runtime intentionally forbids recursive command execution. Use one
journaled swap parent and a private, checked native equipment-application core
shared with the existing system-command path. Preserve left-to-right equipment
effects and expose failures after mutation as integrity failures, not replayable
rejections. The checked native dispatch-reporting overload distinguishes Applied,
Discarded and deferred outcomes; it is groundwork, not that parent command.
The command processor intentionally retains throwing commands. Any composite
that can throw after inventory mutation must invalidate world integrity before
propagating, refuse further mutation on re-entry, and stop publication before
terminal receipts can use stale state. Merely throwing is not an atomic rollback.

The `coop.inventory.owner` channel sends bounded full summary replacements only
to the controlling peer after its baseline ACK. Frames are tied to the current
session, world, baseline, owner and exact actor. Assignment replacement, resync,
disconnect and baseline replacement clear private state and modal choices.
Public actor/delta payloads still carry only their existing equipment projection.
`TacticalInventoryUiHost` stores UI actor identities; it is not the native
inventory mutation boundary.

Each actor has an independent monotonic inventory revision: a reserve-slot change
need not change the public snapshot. The server accepts only the latest staged
and sent private token, then rechecks native state before queueing the command.
The reliable ordered transport sends private replacement state before terminal
receipts unlock input; it reuses the acknowledged baseline recovery barrier, not
a separate private-frame ACK. Sends are limited to two owner frames per peer per
flush with a rotating actor cursor. A pending private replacement is a logical
receipt barrier, not socket backpressure: inbound ACKs must still be consumed,
including when a flush exhausts its message budget. Complex stacks, recursive attachments and
separately referenced LBE containers still need their own bounded representation
and acceptance tests before this can be called complete inventory support.

There is an additional confidentiality limit: campaign synchronization currently
sends the same complete native checkpoint to every admitted peer. An owner-only
live stream cannot hide checkpoint-era inventories from a modified client.
Sanitize or replace that checkpoint transfer before claiming inventory secrecy
or using this bootstrap for adversarial PvP.

Passive artwork uses content-loaded item names/graphics and the authority-selected
portrait descriptor with native 151–154 aliases and camouflage fallback. The
implemented cache draws static STI frame zero in the six visible 48-by-43 face
rectangles without initializing live face objects or consuming simulation RNG.
Absent/unsupported art retains the silhouette. Descriptor codecs, invalidation,
clipping and cleanup have fixture coverage; distinct stock faces have installed
screenshot evidence above.

### 2. First aid through the normal game flow

- Select a medic, equip a kit, approach a teammate, start treatment, and visibly
  follow native bleeding, kit-use and AP changes on both clients.
- Cover obstruction, depleted kits, unsupported actors, ownership, interruption,
  cancellation and reconnect. Distinguish initiation from ongoing treatment and
  completion; never certify healing from an Applied receipt alone.
- Acceptance: an installed two-client test and manual wounded-teammate scenario
  complete this flow using the same shipping controls and authority path.

Start at [client controller](../Ja2/FullEngineCoopClientController.cpp),
[native commands](<../Tactical/Simulation Commands.cpp>), and
[medical services](../Tactical/TacticalActorMedicalSession.cpp).

Native service activity is now replicated separately from bleeding/kit outcomes.
The HUD distinguishes providing and receiving aid, resolves an exact published
patient's current bleeding, and clears departed/replaced patient details. A zero
bleeding value is not a completion signal while native service remains active.
Repeated patient clicks do not become accidental movement; explicit Stop still
cancels native service, with incapacitated recipients prompted to select the medic.
This is presentation of actual authority state, not local treatment simulation,
automatic supply acquisition, self-treatment input or installed treatment proof.

The opt-in installed first-aid verifier requires an already equipped real medical
kit and an eligible adjacent wounded teammate. It submits at most one AID and
requires both replicas to expose the exact service relation, consumed kit points
and reduced bleeding or improved critical health. It does not prepare the scene,
advance turns or infer treatment from initiation. Stock starters without those
prerequisites fail explicitly; unit coverage is not installed medical acceptance.

### 3. Authoritative campaign travel and subsequent encounters

- User-confirmed policy (2026-09-22): any campaign-ready player may order shared
  travel and choose supported battles, hire mercenaries and spend shared funds.
  One designated leader manages campaign time advancement and speed. The
  development default selects the lowest peer identity in the first observed
  campaign-ready cohort and retains that leader across disconnect.
  Strategic pause/speeds and pause-on-leader-loss are implemented with the
  qualification limits above. Shared no-gear hiring now uses the same control
  barrier and passes the pending-hire qualification above. Explicit time-leader
  designation/handover and other shared spending remain open.

- Live campaign phase/time/leader and occupied friendly-group observations are
  implemented. Group identities, membership, sectors, next-leg times and final
  destinations are replicated, as is the exact front pending arrival and its
  native preparation stage. The protocol-14 action API and ordinary client
  Screen now submit supported travel and encounter choices. Full route
  presentation remains open. Qualify observations separately from actual
  travel/time controls.
- Player-driven adjacent travel now uses native movement validation and event
  processing; qualify it with the implemented time controls in the installed
  workflow.
  No teleportation, skipped campaign events or client clocks.
  `CanEntireMovementGroupMercIsInMove` also checks unrelated map-screen-selected
  actors, and `CanCharacterBeAwakened` can mark fatigue collapse even when called
  as an eligibility check. Neither belongs in passive observation capture.
  The native preflight now reuses those underlying rules without these side
  effects, uses isolated fastest-route costs and returns a validated complete
  candidate. The separate native single-leg commit now prepares storage/events
  before mutation and has a real peaceful native-clock/arrival/Lua regression.
  Remote travel requests now reach that single-leg boundary; multi-leg and
  crossing movement still need safe native continuation/rescheduling boundaries.
  `AddWaypointToPGroup` starts movement on the first waypoint; rebuilding paths
  deletes old arrival events before validation. Travel needs a deliberate native
  command boundary, not direct use of these functions as a read-only preview or
  a supposedly atomic network setter.
- Extract a headless-safe encounter-entry action: the existing pre-battle GUI
  callback is not a server API. Preserve encounter, deployment and quest rules.
  Worldless native arrivals now pause into exact pending NPC/battle/coordination
  decisions without opening dialogs, and internal NPC acknowledge/stop is
  supported. Native headless preparation now shares local PBI rules and caches
  the result exactly once for supported surface on-foot battles. It preserves
  ambush/deployment/retreat restrictions and leaves militia reinforcement and
  other unsupported questions pending. A separate explicit entry action now
  supports native forced insertion and player-selected Spread through the
  actual loader. It shares the native placement rules and rejects partial
  failures without replay. Explicit native retreat is also implemented for
  supported on-foot squads, preserving movement events and enemy response while
  leaving time paused. Shared-player requests and client controls now reach
  these supported boundaries. Manual deployment, autoresolve and other native
  contexts remain open. Pending decisions continue blocking time,
  new travel and checkpoints until their supported explicit action succeeds.
- Reuse world draining, fresh-baseline admission and cold checkpoint boundaries
  for departure, arrival, battle entry, victory and subsequent re-entry.
- Acceptance: both players finish the starter encounter, choose a destination,
  travel, enter another encounter and return to shared campaign play. Peaceful
  travel alone or a paused pre-battle screen does not complete this item.

Start at [dedicated runtime](../Ja2/DedicatedCoopRuntime.cpp),
[group identities](../Ja2/StrategicGroupHost.h),
[native pathing](<../Strategic/Strategic Pathing.cpp>),
[movement eligibility](<../Strategic/Map Screen Interface.cpp>),
[campaign clock](<../Strategic/Game Clock.cpp>), and
[pre-battle entry](<../Strategic/PreBattle Interface.cpp>).

### 4. Shared campaign decisions, money and mercenary ownership

- Provide authoritative hiring, contracts, shared funds and roster assignment;
  present the same committed outcomes to both clients.
- The confirmed policy allows any campaign-ready player to hire, spend shared
  funds, order travel and choose supported battles. Initial no-gear AIM hiring,
  its salary/deposit transaction, client controls, and shipping native
  arrival/restore after controlled victory are qualified. Original paid gear
  also has two-client purchase/checkpoint and observed native cold-restore proof
  above; other equipment/contract flows remain open. Complete other
  money/contract transactions under that policy;
  time leadership grants no exclusive campaign-action privilege. Actor
  reassignment and recovery of an offline player's actors still need explicit
  ownership workflows.
- Acceptance: concurrent requests cannot double-spend, overwrite ownership or
  strand a required turn/interrupt indefinitely when a peer disconnects.

Start at [assignment policy](../Multiplayer/CoopActorAssignmentPolicy.cpp),
[session authority](../Multiplayer/FullEngineCoopServerSession.cpp),
[mercenary hiring](<../Tactical/Merc Hiring.cpp>), and
[campaign runtime](../Ja2/DedicatedCoopRuntime.cpp).

### 5. Save/resume at meaningful campaign boundaries

- Prove actual disk save/load for wounded and travelling rosters, subsequent
  sectors, shared funds, inventory and ownership policy—not only classifiers.
- Preserve reconnect identity and avoid command replay across every supported
  resume path; state explicitly when a new server session requires fresh admission.
- Active tactical worlds currently cannot checkpoint: stopping mid-battle can
  retain only the last valid cold campaign generation. This is a real limitation,
  not live-battle persistence. Implement and qualify active-battle saving before
  claiming uninterrupted save-anywhere co-op; until then expose the restriction.
- Acceptance: controlled stop, restart and interrupted-save recovery restore
  the documented checkpoint without hidden rollback, duplication or stalled peers.

Start at [checkpoint eligibility](../Engine/Core/DedicatedCheckpointEligibility.cpp),
[campaign store](../Ja2/DedicatedCampaignStore.cpp),
[save bridge](../Ja2/DedicatedCampaignSaveBridge.h), and
[client transport](../Multiplayer/FullEngineCoopClientTransport.cpp).

### 6. Broader combat, loot, visibility and compatibility

- Close unsupported combat/action flows needed for campaign play, including
  broader weapons, richer world-item pickup/drop placement, loot and relevant interactions.
- Verify per-peer authorized visibility and fog, hidden actors/items, deaths,
  changing geometry and replacement baselines; rendering is not visibility proof.
- Keep single-player and legacy multiplayer regressions separate and tested.
  Any future PvP migration needs its own rules, ownership, secrecy and acceptance.

Start at [tactical observer](../Ja2/TacticalWorldObserverHost.cpp),
[tactical authority](../Multiplayer/CoopTacticalIntent.cpp),
[replica](../Multiplayer/FullEngineCoopSnapshotReplica.cpp), and
[legacy multiplayer](../Multiplayer/client.cpp).

### 7. Full two-client campaign qualification

- Run an extended installed/manual campaign with actual controls: inventory,
  treatment, multiple encounters, travel, hiring, loot, saves and resumes.
- Include independent clients, repeated disconnect/rejoin, late joins, exhausted
  resources and rejected actions. Check host outcomes and both client views.
- Completion requires observed playable outcomes and recoverable failures across
  that flow. Passing unit tests or one isolated successful action is not a substitute.


On 2026-09-29, PRs #900, #901, #903, #904, #905, #906, #907, #908, #909
and #910 merged after all 15 exact-head checks passed. #904's Windows-only
`SendMessage` macro collision was corrected in the fixture; forced-macro
negative/positive checks passed before the corrected CI run. #909 merged as
`a9c2241c9dae61039da3113488e03af20ec913c8`; #910 merged as
`391c4a08dbf11fc276de97dd8b07421094607927`.

The narrow ROOT RINF/SCHD/PCVW integration passed 28 normal and 28 ASan tests,
shipping links, both expected negative controls and architecture (1164.71 s).
Installed run `ja2-coop-legacy-passive-refresh-bwr8f6rp` then passed the actual
legacy upgrade, two-client admission, follower reconnect and cold restart with
seven clean process exits. The original paid-flight generation 15 was cloned;
the shipping ASan host published generation 16 with all six strict sections
before any client launched. Time remained paused at minute 2250, with the same
five mercenaries and balance 11357. Clean shutdown published generation 17;
cold admission retained exactly that save/manifest identity without another
refresh, both clients restored into a fresh epoch, and final shutdown published
18. The source campaign, ancestry, installed inputs, frozen sources, caches and
binaries remained unchanged throughout. The exact generation-16 save/manifest
bytes are preserved alongside the evidence; a separate audit verified every
inspector SHA against its checkpoint record.

This qualifies a worldless legacy checkpoint upgrade and ordinary passive
reconnect/cold resume. It does not qualify active tactical checkpoints, passive
global-state isolation, rendered labels, physical input, first aid or an
unseeded battle. Evidence: `coop-artifacts/2026-09-28/runs/ja2-coop-legacy-passive-refresh-bwr8f6rp/legacy-passive-refresh-evidence.json`
(SHA256 `df30100c6d3df8aeb520e6a24bf9e5588a2b6b3e434a58dd2c176cd68e6f351a`).

The reviewed #909 saved-idle repair has now been applied surgically to ROOT's
five native creation/animation files and its existing fixture. Shipping links
and 14 integrated regressions passed normally (3.70 s) and under ASan (6.26 s),
including saved-idle reconstruction, movement, route continuation, passive
checkpoints and first-aid success/failure cases. The first-aid shipping modal is
separately under review; a queued-key handoff bug at campaign/tactical ownership changes
was found and corrected before integration. No installed first-aid success is
claimed.


## Installed shipping medical continuation (2026-09-29)

PR [#911](https://github.com/tais/1vibe13/pull/911) merged at
`bdf93768c53e830722d84cecca25a122cda2e466` and
[#912](https://github.com/tais/1vibe13/pull/912) at
`ccd216655c13857672adeff40811d22fa736f8b8`, each after all 15 checks passed.
The revision-bound inventory controller is draft
[PR #913](https://github.com/tais/1vibe13/pull/913), exact
`58674d6b9c6c209ae5b788ed97d15c5f6822303b`; independent composition review,
shipping build, three focused tests, architecture and ASan registration pass.
The panel and atomic medical capability remain separately preserved local work
pending the prerequisite merges and their final checks.

The reviewed 12-path ROOT medical Screen/driver integration preserves ROOT's
session protocol 19 and outer tactical wire 6. Shipping server/client builds,
21/21 focused tests normally and under ASan, architecture (1290.14 s), and ASan
coverage (190 executable targets / 164 explicit plus dependencies) pass.
The first ASan attempt had one NotRun because an existing roster-text-plan target
was not built; explicitly building that target yielded 21/21 without source edits.
All 491 unrelated pre-existing dirty/untracked paths were preserved.
Final validation and source/cache/binary hashes are indexed by
`medical-ui-root-graft/root-freeze-summary.json` under the dated artifact root.

Installed run `ja2-coop-shipping-medical-session-c3a1irt7` used the frozen ASan
server and normal installed client, resuming the preserved paid-flight campaign.
Both clients retained the five-person roster and $11357; the nonleader ordered
all five travel legs and native B13 entry. The initial kit swap command 1 was
explicitly rejected with status 2 / reason 9 (**NotBaselineReady**, not
RevisionMismatch). Its inventory and AP remained unchanged. One ordinary owner
reconnect exited the old client cleanly and retained assignments and private
inventory under a fresh baseline (1 to 3). Private cache delivery followed the
first public Ready observation; a later read-only check verified complete caches.
A new explicit kit selection then produced command 2, rejected with status 2 /
reason 12 (**GameplayRejected**), revision 6883. All owned slot data, equipment,
positions, health and kit points remained unchanged, with no AP charge. The
native preflight cause is still unidentified. No movement, fire, end-turn or
first-aid input followed. This is not treatment qualification.

The operator explicitly closed for `inventory-gameplay-rejection` and exited 0.
All three client processes exited 0; the server returned 2 because active-world
checkpointing remains unsupported. The original evidence/private states, assets,
binaries, frozen sources/helpers and ancestry all remained unchanged. The
separate `shipping-medical-attempt-audit.json` verifies rejected-command
conservation and scope. A read-only debugger attach was denied by the OS and did
not access or mutate the process. The next diagnostic must identify the existing
native inventory predicate without weakening acceptance. No physical-input,
paint, first-aid or complete-campaign qualification is claimed.

## Stationary aim poses for inventory and first aid (2026-11-25)

Armed mercs that move in combat walk weapon-ready, and native
`settleIntoStationaryStance` keeps the weapon raised when they stop: they end in
`AIM_RIFLE_STAND` (12) or the crouched/prone/dual-wield equivalent. The swap and
first-aid gates accepted only `STANDING`, `CROUCHING` and `PRONE`, and the swap
also rejected every `ANIM_FIREREADY` state. A rifle-armed merc in combat could
therefore not equip a kit or start first aid until a different-height stance
change, unlike the stock inventory panel and first aid.

The native gates now also accept the six settled aim poses `AIM_RIFLE_STAND`,
`AIM_RIFLE_CROUCH`, `AIM_RIFLE_PRONE`, `AIM_DUAL_STAND`, `AIM_DUAL_CROUCH` and
`AIM_DUAL_PRONE` (`IsStationaryAimPose` in `Tactical/Simulation Commands.cpp`).
The stock hand refresh then settles the pose; replacing a raised rifle while
standing plays `LOWER_RIFLE`. READY/SHOOT/END/RAISE/LOWER transitions and
`ALTERNATIVE AIM` remain rejected, as do all pending-work checks. The client's
medical selection accepts the same poses with a matching stance. Native fixtures
cover crouched-aim kit equip/unequip, the standing rifle-for-kit swap, rejected
transitions, and first-aid preflight from every aim pose for medic and patient
at the same-height idle cost.

This removes the most likely cause of the B13 `GameplayRejected` kit swap above
(first input after hostile entry; the same swap succeeded before combat). That
run's logs no longer exist, so the cause is not confirmed; rerun the installed
medical flow with `JA2_COOP_INVENTORY_DIAGNOSTIC=1` to qualify it.

Transfer, ground pickup and drop now share the same rule through
`IsUnsettledFireReadyPose`: a fire-ready state is rejected unless it is one of
the six settled aim poses, and out-of-range states fail closed. Pickup and drop
have no animation; a transfer that touches a hand reuses the stock hand
refresh. Fixtures cover every aim pose for both transfer roles, pickup at the
same-height idle cost, drop at the stock cost, and rejection of unsettled
fire-ready (`PUNCH_BREATH`, `KNIFE_BREATH`) and firing states.

## Secured sectors stay loaded after victory (2026-11-25)

Previously the dedicated server froze admission and unloaded the world on the
first committed victory, so players could never loot, treat, talk or move in a
won sector. A victory now *secures* the sector instead:

- `StarterMissionState::PeacefulTactical` keeps the world loaded with the
  campaign clock paused (no strategic event can fire while it is loaded;
  tactical movement does not depend on the game clock).
- The campaign status carries a runtime-scoped `securedSector` token (wire
  bytes 121–124, zero otherwise, valid only in the tactical phase).
- Any ready player may end peaceful play with the new campaign action
  `ReturnToMap` (kind 11) bound to that token. In the client, `W` arms it and a
  separate `Enter` confirms (`Esc` cancels); held keys, a stale token, another
  pending request or any modal decision disarms it.
- The runtime only records the accepted request. The frame loop waits until
  every campaign receipt has been delivered and a fresh-baseline boundary is
  reached, then reuses the existing post-combat unload, drain and cold
  checkpoint path. Hostiles reappearing reopen the encounter.

Installed evidence (`tests/full_engine_coop_installed_peaceful_sector.py`, test
host `starter-victory` seeds only enemy incapacitation in a newly created
campaign's starter battle): both clients saw the same secured sector for 12
held samples, moved authoritatively there, client B armed and confirmed the
return through queued Screen input, the server committed the strategic
checkpoint, and the shipping host resumed it cold. Queued Screen input, not
physical OS input or painted pixels; seeded victory, not unseeded combat.

`controlled_battle_return.py --outcome victory` now performs the same explicit
return before expecting the checkpoint. Its other chain probes pin one
historical cohort (profiles, balance) and need the campaign driver before they
can be replayed on a fresh campaign.

### Peaceful sector entry from the map

From worldless strategic play any ready player may load a group's own
player-held sector, surface or underground, with the campaign action
`EnterSector` (kind 12: the exact group plus its displayed sector). On the map,
`E` arms it for the selected group and a separate `Enter` confirms; with a
pending arrival decision `E` keeps answering that decision instead.

`EnterDedicatedCoopPeacefulSector` refuses vehicles, travelling or routed
groups and any sector with hostiles (those go through native arrival and battle
decisions), publishes the map selection, passes `CanGoToTacticalInSector`, and
loads the world with `SetCurrentWorldSector`. The sector is secured on entry:
the same `PeacefulTactical` state and `ReturnToMap` apply, with the won-battle
requirement replaced by the hostile-free proof, and a separate narrow unload
wrapper (`UnloadCurrentWorldForDedicatedCoopPeacefulReturn`) that requires no
hostiles, no combat and no traversal.

Installed evidence: the peaceful-sector probe now also re-enters A9 from the map
after the first return (client A, queued `E` + `Enter`), observes the second
secured sector on both clients, returns through client A, commits the strategic
checkpoint and resumes cold on the shipping host.

### Exit grids (stairs, trapdoors, tunnels)

In a secured sector any ready player may move a whole on-foot group through a
native exit grid with the campaign action `TakeExitGrid` (kind 13: the secured
sector token plus the group). In the client, `U` arms it for the selected
merc's group and a separate `Enter` confirms; at least one member must stand on
the exit grid.

- `PlanDedicatedCoopExitGridTraversal` is read-only: the group must be in the
  loaded sector, every member conscious and controllable, all occupied exit
  grids must lead to one destination, and the destination must be hostile-free
  (hostile levels still need native battle entry).
- The accepted plan is rechecked immediately before leaving; any change
  abandons the traversal and the players stay in the secured sector.
- The squad leaves through the ordinary shared return. Only after the cold
  strategic checkpoint does `ApplyDedicatedCoopExitGridRelocation` apply the
  stock exit-grid insertion (`INSERTION_CODE_GRIDNO` at the destination grid,
  exit-grid re-entry direction) and `SetGroupSectorValue`, worldless: the
  native unload check inside `SetGroupSectorValue` would otherwise tear down a
  live co-op world outside the drain path (a native fixture crashes without
  this guard). The server then enters the destination peacefully.

Miguel's basement is not an exit grid in stock: Fatima's NPC action moves the
squad there with a fade. That belongs to the NPC dialogue work. A10 has no
surface exit grid, so the installed exit-grid step is not exercised yet;
`native_coop_exit_grid_tests` covers planning against a real native exit-grid
table and the worldless relocation.

### Two fixes found by the installed probes

- After an in-process return to the map, player mercs kept a stale in-sector
  flag, so the travel commit refused the next order. Every earlier travel probe
  had cold-restarted first. Both co-op unload wrappers now release the unloaded
  sector's mercs through the native `RemoveSoldierFromTacticalSector`, matching
  a cold load.
- After a confirmed return the client stopped routing the `Enter` key-up to the
  secured-sector input, so a later `Enter` looked like a held repeat. Releases
  now always reach it.

Installed evidence (`full_engine_coop_installed_exit_grid.py`): seeded A9
victory, shared return, in-process travel A9→A10 by the non-leader, leader time
control, peaceful A10 arrival, `E`+`Enter` entry of A10, secured A10, shared
return, checkpoint and shipping cold resume.

## Phase 2.1: talking to NPCs

The server runs the stock talk panel headless (`gTalkPanel`, `Converse`, the
dialogue queue; the game stays paused while a conversation is open, as it does
in single player). Players only ever see a replicated copy of it.

### Conversation channel (read-only)

`coop.conversation` (544 bytes, `Multiplayer/CoopConversation.h`) carries the
one open conversation, or an empty record: its identity, the NPC and merc
(profiles and exact actor identities), the initiating player, whether a line is
still being spoken, and the current subtitle (up to 240 UTF-16 units). The
server observes the panel with `CaptureDedicatedCoopConversation`, which never
opens, advances or closes it, and publishes a new revision to every ready peer
whenever the content changes. Clients draw the NPC's name and the wrapped
subtitle at the top of the tactical view, in place of the medical lane.

### Starting and answering

Two tactical intents drive it:

- `Talk` (kind 15, a 6-byte target identity). In the client, `K` addresses the
  nearest visible non-hostile NPC within the stock talk radius of the selected
  merc. The server preflights it with `PrepareDedicatedCoopTalk`: peaceful real
  time (no combat), no open talk panel (a merc's idle chatter does not block:
  the stock start queues behind it), an exact controllable
  merc and an exact profiled NPC on the same level within `NPC_TALK_RADIUS`.
  It then retains the native `StartConversationCommand`. The requesting peer is
  remembered for exactly that merc/NPC pair and becomes the initiator when the
  panel opens. A panel opened by native gameplay (an NPC walking up) has no
  initiator; then any owner of the merc may answer.
- `Converse` (kind 16: conversation identity plus approach; 1 friendly,
  2 direct, 3 threaten, 4 recruit, 5 repeat, 0 leave). While a conversation is
  open the client is modal, as the stock panel is: only the answering player's
  `1`-`5` and `Esc` do anything, shown as a hint once the line has been spoken.
  The server binds the answer to the published conversation, its merc and its
  initiator, and retains `AuthoritativeConverseCommand` (journal tag 40). At
  execution it re-checks that exact panel is open and idle, then runs only the
  stock button paths: `Converse(...)` or `HandleTalkingMenuEscape(TRUE, FALSE)`.

Unit coverage: wire layouts and validation in `coop_conversation_tests` and
`coop_tactical_authority_tests`, the journal round trip in
`simulation_command_world_object_model_tests`, host translation and
legacy-first rejection in `dedicated_coop_tactical_host_tests`.

Installed evidence (`full_engine_coop_installed_npc_dialogue.py`): after a
seeded A9 victory, client A walks its selected merc to Fatima (the team must
see her first; the test host logs NPC positions so the probe knows where to
walk) and presses `K`. Both replicas show "Why are you here?". Only A may
answer, and A's `1` brings her friendly reply ("How could it be that you were
sent by Enrico? ...") on both replicas, then `Esc` closes the panel on both.
Negative control: with the answer executor disabled the panel opens but the
line never advances and the probe fails at that step.

Not yet: NPC actions such as the basement fade and recruiting, and a choice
among several NPCs in range (`K` always takes the nearest). The server plays no speech audio; the speaking flag is
the native dialogue state (a line active or queued), which the stock
subtitle-only timing ends.

## Phase 2.3: giving items to NPCs

`GiveItem` (intent kind 17: NPC identity, source slot, inventory revision)
hands the whole object in one hand or pocket slot to an adjacent NPC, the way
dropping the cursor item on them does in single player. In the client: open the
inventory, click the item, press `K`; the nearest visible NPC is the recipient.

The server prepares `AuthoritativeGiveItemCommand` (journal tag 41) against
the client's exact inventory revision, the merc's pose and fingerprints, the
source object's fingerprint and the NPC's grid. At execution it re-checks all of
it plus peaceful real time, no open talk panel and adjacency, and refuses
shopkeepers (their give opens the shop GUI, which co-op does not support yet)
and recruited RPCs. Then it runs the stock path:

- The authority has no item cursor, so the source slot is emptied first and the
  object travels only in the native pending give (`SoldierGiveItem`). If no give
  pends, the object goes straight back into its slot; a mismatch marks the
  world invalid.
- The NPC keeps or returns the item in its native conversation
  (`APPROACH_GIVINGITEM`). Two native fallbacks would put a returned item on
  the item cursor; in dedicated co-op it goes to the giver, or their tile when
  their inventory is full. A give that fails during its animation does the same.

Installed evidence (`full_engine_coop_installed_npc_dialogue.py`, extended):
after the Fatima conversation, the client owning Enrico's letter opens its
inventory, clicks the letter, presses `K`; the letter leaves the merc's
inventory and Fatima's panel opens on both replicas with "Hmmm... A letter from
Enrico Chivaldori. He has not forgotten us...". Negative control: with the give
executor discarding, the client gets a terminal Discarded receipt, keeps the
letter and the probe fails at that step.

Two observations from repeated runs:

- A `K` pressed right after a walk is sometimes refused as `NotBaselineReady`
  (the client has not yet acknowledged the latest world revision). Nothing
  executes; pressing again works. The probe retries such refusals, as the
  movement probe already does. A client that held the key until caught up
  would be friendlier.
- Merc chatter (a line playing with no talk panel open) is frequent while
  paused in a secured sector. Talk and give first also required that no line
  be playing; single player refuses only while a talk panel is open (the stock
  conversation start queues behind chatter), so co-op now does the same.
- After a completed give the spent `MERC_GIVEITEM` stayed pending (stock
  clears it with the next UI command), and the co-op move gate refuses any
  merc with a pending action, so the giver could never move again. In
  dedicated co-op the give now clears its own spent action. The probe follows
  Fatima after "Follow me" to prove it: the giver walks up to her and the stock
  PC-near trigger leads her on (negative control: without the clear the walk is
  refused and the probe fails there).
- Two early anomalies (a give that never got a receipt, then five gives
  discarded exactly as under the negative control) were stale test hosts: the
  default `ninja` target does not relink `full_engine_coop_installed_entry`,
  so those runs used a server built before the give existed, then the control
  build. Probe binaries must be built by name. The give resolver also names
  each refusal stage under `JA2_COOP_INVENTORY_DIAGNOSTIC=1`.

## Phase 2.4a: following Fatima into A10

Fatima's quote records (the test host prints them) script the rest of the
Omerta opening. After "Follow me" she walks a chain of waypoints in A9; at each
she waits until a merc is within twice the talk radius (`PCsNearNPC`, the
stock named-NPC AI). Her last A9 record tags her `QUOTE_ACTION_ID_TRAVERSE_EAST`:
she does not walk off the map herself; when the squad leaves the sector, the
stock unload (`HandleDefiniteUnloadingOfWorld` -> `HandleNPCChangesForTacticalTraversal`)
moves her profile one sector east. The co-op unload wrappers run that same
native unload, so co-op needs nothing new for the crossing: the players return
to the map, travel to A10 and enter it peacefully, and she is there, back on
her script (her done-traversal record waits for a merc again).

An earlier note called her stop at the second A9 waypoint "not understood":
that is this design, not a stall.

Installed evidence (`full_engine_coop_installed_npc_dialogue.py`, extended):
after the letter the giver follows Fatima across her A9 waypoints until she is
tagged to traverse east; the squad returns through `W` + `Enter`, the
non-leader orders A9 -> A10, the leader runs time, `E` + `Enter` enters A10,
and Fatima is standing at her A10 entry grid (17806). The A9 -> A10 travel and
entry steps are now a shared helper (`travel_and_enter_a10`), also used by the
exit-grid probe, which still passes.

## Phase 2.4b: the hideout cutscene as a co-op traversal

In A10 Fatima leads on through her waypoints (17806, 17188, 11911, 9177, 8373)
the same way, and at 8373 fires `NPC_ACTION_GOTO_HIDEOUT`. Natively that action
moves the current squad with `SetGroupSectorValue` (whose unload check tears
down a live world outside the co-op drain path), fades out, loads the basement
directly, edits the surface map (crate moved, trapdoor added) and starts
Carlos's conversation on the fade-in. On the dedicated co-op server:

- The action keeps its stock bookkeeping: the exit grid it adds at the hideout
  entry (recorded in the map temp file, so players can always take it later
  with `U` + `Enter`), and Fatima and Dimitri moved to the basement. Instead of
  the squad move and fade it only flags a request.
- The runtime takes the request during peaceful play and plans a scripted
  traversal (`PlanDedicatedCoopScriptedTraversal`): the group of the conscious
  merc nearest Fatima (the squad that followed her; stock's "current squad"
  means nothing on the server), the same whole-group readiness as an exit grid,
  the stock destination and the stock `BASEMENT_ENTRY_n` grid for each member.
- It leaves through the shared return like an exit grid: re-planned
  (`ReplanDedicatedCoopTraversal`) just before leaving, cold checkpoint, then the
  worldless relocation with per-member grids and no exit-grid facing. Only then
  does it apply the stock surface edits to the unloaded A10 map, and once the
  basement is secured it starts Carlos's record 1, the stock fade-in welcome.

Native coverage (`native_coop_exit_grid_tests`): scripted planning without an
exit-grid tile, per-member grids (the last reused), refused destinations and
grids, readiness, replanning of both plan kinds, and a scripted relocation.

Installed evidence (`full_engine_coop_installed_npc_dialogue.py`, extended):
from the A10 entry the probe keeps a merc near Fatima at each waypoint; at 8373
the server logs "hideout traversal accepted to (10,1,1) with 4 mercs", returns
through the checkpoint, relocates the group, enters the basement peacefully and
starts the welcome, and both clients show Carlos's conversation ("Do not move
and no one will die."). Negative control: with the deferral disabled the native
cutscene runs, the clients lose their world and the probe fails.

## Phase 2.4c: Miguel reads the letter

No new authority code: after Carlos's welcome the stock basement chain plays
by itself. Carlos, Dimitri, Fatima, Carlos and Miguel each open a conversation
(NPC-opened panels, answerable only by the owner of the addressed merc), every
line is spoken on both replicas and each panel closes on its own; Fatima's
`NPC_ACTION_FATIMA_GIVE_LETTER` hands the letter to Miguel NPC to NPC through
the stock give, and he reads it, setting `FACT_MIGUEL_READ_LETTER`. That fact
is what lets Omerta's loyalty rise and militia training start (Phase 3).

Installed evidence: the dialogue probe now ends when the test host reports
fact 23 set, after "I do not... Let me see this letter." and Fatima's "Here...".
The whole Omerta opening, from the first word to Fatima to Miguel reading the
letter, now runs through co-op Screen input on the dedicated server.

### Scripted NPC-to-NPC scenes

After Fatima's "Here..." the chain goes on (Ira, then Miguel), but the talk
panel's other party is now Fatima, not a player merc. Capture used to skip such
panels, so the replicas showed nothing while the server kept the last panel
open, waiting for the stock "Done", and every later talk was refused as busy:
a dead end. Now:

- The conversation record carries an NPC-source flag (byte 60; a strict
  boolean, reserved bytes 61-63 stay zero), and capture accepts any profiled
  in-sector soldier as the other party when it is not a player merc.
- Any player may close such a scene, and only close it: the client sends the
  leave with one of its own mercs as the actor; the server accepts only
  approach 0 for an NPC-source panel; `AuthoritativeConverseCommand` now names
  the panel's source profile, and the executor refuses anything but a leave
  when that source is not the commanding merc and not a player merc.

Installed evidence: after the letter the probe closes every idle panel; the
last one (Miguel to Fatima: "Find him and tell him no more than you need
to...") is flagged as a scene and is closed by the other client, after which
the basement is quiet.

## Phase 2.4d: recruiting Ira

With the basement quiet, a player walks up to Ira, presses `K` and answers `4`
(recruit). The stock records accept ("All right. I could be of help...") and
`RecruitRPC` moves her onto the player team. Stock does that by creating a new
player-team soldier (a new actor identity) and removing the civilian one. The
server already handled this correctly: the new controllable actor changes the
assignment set, every peer gets a fresh baseline carrying the new assignments
together with the snapshot that first shows her, and the round-robin policy
gives her to one player.

The client did not. Its presentation checked the new assignment list against
the passive UI model of the previous revision, before synchronizing the new
snapshot, found no such actor and rejected the presentation ("renderer proxy
world does not match snapshot"). It now stores the assignments and leaves their
validation to the interface synchronization of the revision that introduces
them; the incremental check remains for assignment changes within an already
synchronized revision.

Whether Ira accepts is stock logic: it depends on the recruiting merc
(leadership, personality, her opinion) and town loyalty, so a refusal ("Sounds
interesting, but I'm not sure of you.") is a legitimate outcome. The probe
recruits like a player would: if she refuses, it closes her panel, selects the
next merc with `Tab` and tries again, first the acting player's mercs, then the
other player's. In the passing runs so far the first merc was accepted, so that
retry path is not yet exercised. `K` addresses the nearest visible NPC, and a
bystander (profile 114) sometimes stands nearer than Ira; the probe first moves
until Ira is the talk target.

Installed evidence: the dialogue probe ends with Ira on the player team on both
replicas and in exactly one player's assignments. The run before this fix, with
the same probe, failed at exactly that step with the presentation rejection.
