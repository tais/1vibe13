# Laptop code walkthrough and safety audit

This is the living audit record for the legacy `Laptop` application domain.
It complements the runtime campaign extraction in
`CAMPAIGN_RUNTIME_STATUS.md`: safety fixes should strengthen the same ownership
and test boundaries rather than accumulate as unrelated local workarounds.

## Scope and method

The initial structural pass covered all 98 Laptop translation units listed by
the build, approximately 81,000 lines across 187 source/header files. It used:

- all-host compilation of the JA2, Unfinished Business, and Map Editor Laptop
  targets;
- Clang static analysis over every Laptop compile command;
- manual tracing of campaign branches, dynamic allocation, iterator/index
  boundaries, callback lifetimes, localized string buffers, and resource-load
  error paths;
- data-free policy/boundary tests plus a fresh AddressSanitizer test build.

This is a staged full-domain audit, not a claim that one analyzer pass proves
the absence of defects. Each completed batch records confirmed faults and
leaves the remaining areas explicit.

## Communications and commerce batch

The first batch extracts email, insurance, and shipment decisions into
`CampaignLaptopCommunicationsPolicy` and fixes the following confirmed faults:

- `AddEmailWithSpecialData` compared an unsigned XML ID with `-1`, so the
  default no-XML value could be treated as a real XML index whenever XML mail
  was loaded.
- UB XML shipment text was loaded correctly and then overwritten by an
  uninitialized EDT buffer.
- UB's IMP-results and Bobby Ray shipment records both use offset 198; the old
  numeric switch could invoke IMP result handling for shipment mail.
- `PreProcessEmail` compared an uninitialized record pointer after a
  non-terminating assertion.
- insurance mail routing contained an always-true `A != X || A != Y`
  condition, while payout/event entry points trusted null actors, corrupt
  profile IDs, and invalid payout indices after non-terminating assertions.
- direct `MemRealloc` assignment could lose the existing payout allocation on
  failure. Expired/corrupt insurance dates could also underflow into a huge
  unsigned payout, and inconsistent active-slot counters could write past the
  payout array; both paths now fail or clamp safely.
- PostalService used `index > size` at four shipment boundaries, dereferenced
  end iterators before checking them, indexed delivery methods at their exact
  end, and computed a strategic-map index before validating coordinates.
- shipment manipulators and zero-result item arrays leaked; delivery-method
  tables had raw owning pointers with no deletion path. Delivery tables now
  use value ownership and temporary arrays/callback manipulators use RAII.
- starting a new campaign cleared shipments but retained their used-ID bitmap.
- Bobby Ray's scroll-region refresh could delete an array, skip reallocation
  when its size was unchanged, and immediately register regions through the
  freed pointer. Shrinking to a non-scrollable list also left stale regions.
- item-image rendering continued after failed graphic/video-object loads and
  could dereference absent 8/16/32-bit image storage.
- Intel map purchases could shift by a negative dropdown key; Intel and PMC
  labels wrote the multi-character literal `'/0'` instead of a terminator.
- A.I.M. biography buffers could be read after failed localization/EDT loads.
- Bobby shipment selection and M.E.R.C. gear-count access accepted exact-end
  indices.
- Personnel initialization assumed actor slot zero existed, then reused the
  last resolved actor in the team-loop condition; an empty slot could abort
  page setup or be dereferenced on the next iteration. Team bounds are now
  captured from `OUR_TEAM`, missing actors are skipped, and current/departed
  display mode is snapshotted before resolving live actors.

The policy test covers both campaign catalogs, every insurance-template
availability branch, shipment records, IMP offsets (including 198),
negative/exact-end index cases, non-scrollable divisors, and missing record
lookups. It also verifies that departed-personnel rendering never invokes a
live-actor resolver and that missing current actors remain safely nullable.
Architecture CI pins runtime callers, common build ownership, and the
AddressSanitizer target.

## Bobby Ray commerce ownership batch

The second cohesive batch moves Bobby Ray's capacity, count, stock, and
visibility rules into the dependency-free `BobbyRayCommerceModel`, while the
legacy adapters continue to own the established save records and game-facing
types. It fixes the following confirmed faults:

- both inventory initializers wrote an end sentinel after a potentially full
  `MAXITEMS` array;
- the external maximum-purchase setting drove loops, clears, copies, and
  package construction over fixed 100-entry cart and order arrays;
- corrupt old-order counters could request inconsistent allocations, and the
  migration allocated by a trusted active count before copying a different
  number of records;
- legacy order and shipment loads published raw allocations before all reads
  succeeded, leaking or retaining partial state after a short/corrupt file;
- PostalService trusted serialized structure sizes, shipment/package counts,
  IDs, item indices, quality, and duplicate shipment IDs, while destroying the
  current shipment list before validation completed;
- placing an order could open the shipment page with a stale pointer snapshot,
  and region removal used the changing live count instead of the number of
  regions actually created;
- shipment selection bounded iteration by the number of active records rather
  than the list size, so delivered records before an active shipment made rows
  unselectable;
- a missing catalog lookup stored the exact-end `MAXITEMS` index for later use,
  and same-class inventory lookup treated an item ID as a sorted-list slot;
- inventory list lengths, package item IDs, and purchase slot IDs were trusted
  at several render/update boundaries; and
- stock subtraction could underflow while incoming stock could wrap the
  byte-sized quantity counter.

Cart configuration is now clamped at the legacy physical capacity; inventory,
order, and shipment lengths are normalized before use; stock operations
saturate; and both legacy mail-order and PostalService loads build temporary
owned collections and swap them into live state only on success. The save
format and its fixed structures remain unchanged. `BobbyRMailOrder.cpp` also
moves into the campaign-neutral Laptop partition, which now contains 72 shared
translation units and 26 application variants. Its two superseded shipment
implementations and their raw-reallocation/configuration-sized loops have been
removed, leaving the validated PostalService path as the sole implementation.

The focused headless test covers zero, exact-end, oversized, negative,
inconsistent-count, byte-underflow, byte-overflow, visibility, and active-count
derivation cases. Architecture CI pins every production adapter, the shared
build ownership, retired sentinel writes, the focused target, and its ASan CI
admission.

The targeted analyzer rerun also exposed two adjacent save-migration leaks:
the pre-v101 dealer-inventory matrix escaped on short reads, including failures
inside its nested special-item stream, and an oversized/truncated email subject
escaped before it was attached to an email node. Both temporary allocations
now have scoped ownership across every early return, and the architecture check
pins those ownership guards.

## XML and localization input boundary batch

The third cohesive batch puts all 13 Laptop XML readers behind the
dependency-free `LocalizationInputModel` and the legacy-facing
`LocalizationInputAdapter`. Expat callbacks still adapt the established XML
schemas and game structures, but they now build temporary records and publish
them only after the complete document and every record have validated. The
batch fixes the following confirmed faults:

- callback text chunks used truncating `strncat` calls whose bound differed
  from several actual buffers, while failed UTF-8 conversion was ignored and
  could publish empty or stale fixed-buffer text;
- `atol`, `atoi`, and `strtoul` results were narrowed into byte, signed-byte,
  boolean, and index fields without distinguishing the documented `-1` byte
  sentinels from other negative, overflowing, trailing, or exact-end values;
- most readers wrote global arrays, vectors, and PostalService collections
  from inside callbacks, so malformed or truncated documents left partially
  replaced live state even though the reader returned failure;
- repeated IMP voice loads appended duplicate records, and missing fields
  inherited values from the previous voice because the current record was not
  reset;
- Old A.I.M. archives accepted index 255 for the 255-element profile array;
- localized shipping-destination lookup dereferenced `begin()` before checking
  an empty list and continued past `end()` after a missing external ID;
- briefing-room XML supplied an unbounded index to a raw caller array, accepted
  page/image counts beyond its four-entry layout, and silently skipped the
  `SecretCode` element because only its closing callback recognized the tag;
- delivery methods mutated PostalService while parsing and retained a raw
  callback pointer to the current method; all external destination IDs and
  duplicates are now resolved before any method is installed;
- localized email documents could address missing email/message records and
  then commit an incomplete overlay; base and localized reads now require the
  complete staged shape before swapping it into `gEmails`; and
- the adjacent briefing biography reader leaked its file on seek/short-read
  failures and passed byte counts to a character-count decoder, including the
  biography size for the smaller additional-info record.

The shared model accumulates arbitrary callback chunks without partial writes,
parses integral values with range-preserving `from_chars`, accepts only 0/1 as
legacy booleans, explicitly maps only documented `-1` byte sentinels to 255,
checks signed and exact-end indices, and copies fixed arrays without modifying
the destination on failure. UTF-8 conversion likewise uses a temporary fixed
array and commits only after capacity and conversion checks.
Focused data-free tests cover exact capacity, overflow, unterminated buffers,
malformed and out-of-range integers, strict booleans, signed/exact-end indices,
and copy failure without mutation. Architecture CI applies the adapter and
transactional-state requirements to every `Laptop/XML_*.cpp`, rejects the
retired conversion/narrowing calls and mutable parser-mode globals, pins the
specific crash boundaries, and admits the focused target to ASan CI.

## IMP creation lifecycle and import transaction batch

The fourth batch gives IMP navigation and bounded selections a dependency-free
`ImpCreationStateModel`, while `CharProfile` remains the legacy-facing owner of
page entry/exit and rendering. It fixes the following confirmed faults:

- any Laptop source could assign the current IMP page, including negative or
  exact-end values; the non-terminating assertion did not prevent the visited
  array from being indexed with the invalid value;
- the visited-page reset stopped at `IMP_CONFIRM`, leaving every later IMP page
  with state inherited from an earlier creation session;
- portrait-next indexed after incrementing past the array, portrait cycling
  recursively skipped the other sex forever when mod data had no match, and
  an empty selection could pass an uninitialized filename to video loading;
- voice selection fell back to index zero even for an empty or sex-incompatible
  voice catalog, and rendering and completion later indexed it as valid;
- exhausting all IMP profile slots returned `-1`, which character creation then
  used as an index into `gMercProfiles`;
- XML eye and mouth coordinates were mistakenly treated as indices into a
  retired 50-by-4 offset table, allowing custom coordinates to address outside
  the table instead of being copied as values;
- repacking a smaller portrait XML document retained enabled entries from the
  previous load, and a new base document retained omitted records from the old
  base state;
- IMP import leaked its file on several short-read paths, changed the global
  save version and live profile before validation completed, trusted the saved
  portrait and profile type, and charged the player before confirming that the
  merc could be hired;
- creation marked IMP complete and charged the player without checking that a
  free profile was created and hired successfully;
- import/export and existence checks concatenated user-controlled nicknames
  into 13- and 32-byte arrays, sometimes without a terminator; and
- starting another IMP retained attribute increments, trait lists, portrait,
  voice, selected gear, gear cost, and the new-gear mode from the previous
  merc; and
- IMP pages passed player-entered names and activation codes, XML voice names,
  and localized strings directly as variadic `mprintf` formats, so `%`
  sequences could consume absent arguments and crash or corrupt rendering.

Navigation now has one validated owner and read-only compatibility view.
Portrait, voice, and preferred/free-slot searches are bounded, wrap explicitly,
and return no value for empty catalogs. Profile construction validates names,
slot, and portrait before initialization; face coordinates are copied directly
from the validated XML record. Import reads into a temporary initialized
profile through a scoped file handle, temporarily adapts the saved version,
validates the complete record, computes affordability without publishing the
slot, and rolls back live state if hiring fails. Finance, history, completion,
and persistence side effects occur only after successful hire. The on-disk IMP
format and portrait XML schema are unchanged.

The focused headless model test covers negative and exact-end pages, reset and
visited transitions, signed indices, empty/no-match catalogs, forward/backward
wrap, invalid current selections, preferred free slots, occupied preferred
slots, and fully occupied slot sets. Architecture CI makes `CharProfile` the
only page writer, pins the staged import and rollback path, rejects the retired
coordinate table and fixed filename buffers, and includes the focused target
in the AddressSanitizer build. It also rejects three-argument `mprintf` calls
whose format is a variable anywhere in the IMP page cluster.

## File viewer and history runtime-content batch

The fifth batch moves the complete remaining `files.cpp` and `history.cpp`
campaign-content seam behind the dependency-free
`CampaignLaptopContentPolicy`. It fixes four confirmed defects:

- when a UB installation did not contain `RIS25.edt`, the file viewer's
  fallback branch attempted to load the same missing file again instead of
  using `RIS.edt`; and
- when `quests25.edt` was absent, a completed UB quest loaded the even
  quest-start record from `quests.edt` instead of its paired odd completion
  record; and
- a killed-merc history record with `NO_PROFILE` only initialized its rendered
  text in beta application builds, leaving the shared/release path with stale
  output; and
- writing an empty history page returned after opening the history file but
  failed to close its handle.

The policy selects the 68-record Arulco or 39-record UB briefing view, its
installed-data fallback, Arulco/Tracona map artwork, Arulco-only biography
artwork, and exact quest start/completion record. The page implementations
still own asset probing, encrypted-text loading, and rendering. Both are now
compiled once in the shared Laptop partition, which contains 74 common
translation units and 24 application variants, and neither contains a
compile-time `JA2UB` identity.

The focused data-free test covers both campaigns, present and absent optional
assets, exact briefing counts, map and biography selection, and paired quest
record indices. Architecture CI pins every policy caller, shared build
ownership, both corrected fallbacks, the compile-guard reduction, and ASan CI
admission. The missing-profile history diagnostic is now unconditional, and
the empty-page early-return close is pinned as well.
Installed text/artwork names and briefing, history, quest, save, XML, Lua, and
package formats are unchanged. The all-host warning pass also removes an empty
Laptop declaration, mistaken `extern` definitions, redundant comparison
wrappers, a nested comment marker, and behavior-neutral dead stores encountered
while rebuilding and statically analyzing the shared and per-application page
partitions. The targeted path-sensitive analyzer is clean for every production
translation unit changed by this batch.

## A.I.M. page resource ownership and re-entry batch

The sixth batch introduces the dependency-free `ResourceHandleSet` transaction
and the legacy-facing `LaptopPageResourceOwner`. Numeric video, surface,
button-image, and button handles plus static mouse-region registrations are
acquired into a staging owner. A page publishes the complete owner only after
every fallible acquisition succeeds; an early return therefore releases the
partial set. Teardown always removes regions and buttons before unloading their
images and render assets. `UniqueResourceHandle` now also models APIs whose
invalid value is signed `-1`, where handle zero remains valid.

The A.I.M. landing page, defaults, navigation bar, Archives, Facial Index,
History, Links, Members, Policies, and Sort now use this boundary. The Members
conversion includes its transient portrait, gear controls, modal confirmation,
video-conference title surfaces, close control, and every conference-mode
button group. Confirmed faults fixed in this batch include:

- every chained page entry leaked all earlier video objects, regions, images,
  and buttons when a later acquisition failed;
- Archives never released `guiPageButtons`, and removed all 20 Alumni regions
  even when only sparse profile records had registered a subset;
- several pages unloaded shared button images before removing their buttons;
- Facial Index registered each portrait region before its matching portrait
  load, then removed regions based on a different profile-count bound;
- Archives indexed page 3 through a three-entry visited array and History
  indexed page 5 through a five-entry array; both pages plus Policies also
  trusted a persisted negative or exact-end current-subpage value;
- failed Members video-conference transitions published the new mode despite
  partial creation, leaked intermediate mode resources, and could exhaust the
  face pool; transitions now commit their mode only after complete creation and
  remain retryable after failure;
- the Members confirmation popup disabled its close control before fallible
  loads and copied caller text into two fixed 400-character buffers without a
  bound; and
- the Policies agreement controls were destroyed and recreated on every render
  of the agreement page.

`AimFacialIndex.cpp`, `AimLinks.cpp`, and `AimMembers.cpp` no longer depend on
application compile identity and join the common Laptop object partition. The
partition now contains 77 common translation units and 21 per-application
variants. Focused headless tests cover signed invalid sentinels, valid handle
zero, failed acquisition without publication, staging rollback, committed
transfer, and exactly-once release. Architecture CI pins the owner layers,
every migrated page, common build ownership, and rejects restoration of raw
resource acquisition or teardown in the cluster. All-host builds, the complete
headless suites, sanitizer builds, and targeted static analysis remain required
before merge.

## M.E.R.C. site ownership, navigation, and video batch

The seventh batch applies the same transaction to the complete M.E.R.C. site:
landing, Files/gear, Account, and No Account. Page backgrounds now participate
in each page transaction rather than committing before later fallible loads.
The Files transaction includes its five gear-kit buttons and all 21 inventory
help regions; its transient portrait is scoped to the render call. Speck's
close control and subtitle region have independent owners because their
lifetimes follow video/dialogue state rather than page entry.

Confirmed faults fixed by this batch include:

- every M.E.R.C. page leaked all earlier objects, images, buttons, and regions
  when a later page acquisition failed;
- Files never unloaded `guiMercWeaponKitButtonImage`, while several exits
  unloaded button images before removing the buttons that referenced them;
- Files could decrement `gubCurMercIndex` from zero to 255 while skipping an
  alternate profile, and accepted stale saved selections beyond the configured
  available roster;
- the visible-roster resolver assumed condition slot zero was the first
  available merc, so a sparse availability list selected the wrong profile;
- Speck video startup used a failed `InitFace` result as an index, retained a
  close button after failed startup, and dereferenced failed framebuffer locks
  in non-asserting builds;
- the distortion scan compared a relative row with an absolute screen
  coordinate, leaving most of a distortion cycle inert;
- Account callbacks could underflow or advance beyond their page count if
  invoked despite disabled controls; and
- `ExitMercs` set `gfInMercSite` to true, leaving the site marked active after
  teardown.

The final John-availability compile branch now uses `CampaignMercSitePolicy`.
All four sources therefore join the campaign-neutral Laptop partition, which
now contains 81 common translation units and 17 per-application variants.
Data-free tests cover empty, stale, exact-end, and alternate-profile navigation;
architecture CI pins the failure checks, runtime policy, owner use, common
manifest, and absence of raw resource lifecycle calls. All-host builds, the
complete headless suites, sanitizer builds, and targeted static analysis remain
required before merge.

## Florist and Funeral service-site batch

The eighth batch deliberately groups the complete four-page Florist workflow
with Funeral, whose primary action enters that workflow. Each page now stages
its default title/background, page graphics, button images, buttons, and mouse
regions as one transaction. The flower gallery owns its replaceable image and
button set separately, while the order form independently owns delivery rows
whose lifetime follows the open drop-down rather than the page.

Confirmed faults fixed by this batch include:

- all five pages leaked earlier acquisitions when a later graphic, image, or
  button failed, and several exits released button images before their buttons;
- the order form allocated its destination-region array only once, then rebuilt
  its shared destination table from mutable XML on later visits, allowing a
  larger catalog to write past the old allocation;
- leaving while the delivery drop-down was open retained its dynamically added
  regions and static "created" flag, while text-field teardown attempted to
  read fields after input mode had already been killed;
- an empty destination catalog and stale destination or flower selection were
  dereferenced by rendering, pricing, drop-down, and purchase callbacks;
- stale gallery starts could underflow the remaining-item calculation, and the
  previous exact-end workaround relied on a hard-coded visited-array index;
- malformed flower price text left an uninitialized value in both gallery and
  order rendering;
- localized card or Funeral link text taller than its container underflowed an
  unsigned centering offset; and
- the order form's Meduna scene still depended on `#ifdef JA2UB`.

`FloristSiteModel` now owns the dependency-free selection, page, and centering
bounds. `CampaignLaptopCommunicationsPolicy` selects the delivery scene at
runtime, allowing the order form to join the common build and moving Laptop to
82 common translation units and 16 variants. Focused tests cover empty, stale,
exact-end, partial-page, campaign, and tall-text cases; architecture CI pins
the dynamic owner, storage refresh, text lifecycle, runtime policy, common
manifest, and raw-lifecycle exclusion. All-host builds, the complete headless
suites, sanitizer builds, and targeted static analysis remain required before
merge.

## Insurance site ownership and policy-transaction batch

The ninth cohesive batch covers the Insurance home, information, comments,
and contract pages as one service-site boundary. Each page now stages its
shared defaults together with its own video objects, button images, buttons,
and regions before publishing a complete entry. The contract page owns its
replaceable form controls separately, and mercenary portraits use a scoped
temporary handle.

Confirmed faults fixed by this batch include:

- every page leaked earlier defaults or page resources when a later
  acquisition failed, and the information/contract exits unloaded button
  images before removing the buttons that referenced them;
- the information-page visited array allocated four entries for five pages,
  so visiting the cancellation page wrote one byte past the array;
- the contract page indexed a fixed, partially refreshed profile array using
  mutable global pagination state, allowing stale entries and exact-end page
  indices after the team changed;
- form controls used a static creation flag and the current rather than the
  originally created form count during teardown, making failed and repeated
  replacement unsafe;
- accepting a policy charged the player and then restored the old policy
  length, discarding the purchased extension;
- an insufficient-funds purchase changed the policy start fields and always
  added the requested length after displaying the rejection; extensions also
  skipped the final balance check;
- an expired employment contract returned a negative day count as a huge
  unsigned insurance term;
- corrupt payout usage counts or missing backing storage could drive
  allocation/publication from an inconsistent state, payout multiplication
  could overflow, and end-game shutdown left stale capacity counters behind;
  and
- a failed framebuffer lock was passed to the Insurance line renderer.

`InsuranceSiteModel` now owns dependency-free roster pagination, information
page bounds, purchase prerequisites, saturating coverage arithmetic, expired
employment handling, and payout storage/count consistency. Purchase and
extension callbacks re-resolve their selected actor and validate the complete operation
before finance, history, or employment state is changed. The existing
insurance records, premium formula, campaign communications policy, save
layout, and common Laptop build ownership remain unchanged. A dedicated
headless target covers empty/stale/exact-end rosters, final partial pages,
purchase rejection, extension saturation, expired contracts, invalid profile
sentinels, and corrupt payout storage/counts; architecture CI pins the owner
boundaries, dynamic replacement, retired stale state, test target, and absence
of raw lifecycle calls.

## Bobby Ray catalogue ownership and input-safety batch

The tenth cohesive batch covers the Guns, Ammunition, Armour, Miscellaneous,
and Used catalogue pages together with their shared title, filter bars,
catalogue menu, navigation controls, and item-image interaction regions. Each
page now stages its background/grid graphics and the complete shared control
set through `LaptopPageResourceOwner`; callback-replaced item regions have an
independent owner, and transient item graphics use a scoped video handle.

Confirmed faults fixed by this batch include:

- failed entry could leak any successfully created background, grid, title,
  filter, or menu resource, while exits unloaded button images before removing
  the buttons that referenced them;
- Used and Misc skipped their LBE filter under the old inventory system without
  clearing the corresponding button ID, allowing a prior visit's stale control
  to be updated;
- item-image regions were registered and removed with raw mutable counts, the
  mouse wheel inspected all four slots even when fewer regions existed, and
  queued callbacks trusted their slot after a page/filter replacement;
- keyboard item hotkeys inferred visibility from page remainders, including a
  zero-page underflow path, instead of the regions actually created;
- an empty catalogue rendered page `1 / 0`, retained a stale final-page
  remainder, and page arithmetic narrowed the result through an eight-bit
  counter and floating-point division;
- temporary item graphics leaked when object lookup or bit-depth validation
  failed; and
- stale/corrupt inventory item IDs and malformed LBE class, pocket-vector,
  pocket-definition, or attachment-mapping indices could address beyond loaded
  data or perform an invalid bit shift.

`BobbyRayCatalogueModel` now owns dependency-free integer page counts, stale
page normalization, bounded next/previous jumps, empty-page presentation,
visible-slot checks, and exact-end index validation. The live catalogue uses
those rules for buttons, keyboard input, mouse-wheel input, callbacks, and LBE
rendering. Existing inventory records, filters, artwork, purchasing behavior,
save layout, campaign policy, and the 82-common/16-variant Laptop partition are
unchanged. A dedicated headless target covers empty/full/partial and more than
255 pages, stale navigation, hidden slots, and exact-end indices; architecture
CI pins all five owners, shared helper ownership, dynamic-region replacement,
scoped temporary graphics, test admission, and raw-lifecycle exclusion.
The focused analyzer pass over all three `BobbyRGuns.cpp` host variants and
the four shared catalogue implementations is clean; obsolete layout-result,
image-dimension, and keyboard-state dead stores found during that pass were
removed.

## Bobby Ray fulfilment ownership and list-safety batch

The eleventh cohesive batch completes the connected Bobby Ray home,
mail-order, and shipment pages. Each page now stages its wood background,
graphics, title link, buttons, button images, and fixed regions through
`LaptopPageResourceOwner`. The mail-order destination drop-down, order-grid
scroll controls, and shipment-row regions use independent replaceable owners
with stable vector-backed region storage.

Confirmed faults fixed by this batch include:

- failed entry could leak any resources created before the failing load, and
  both order and shipment exits unloaded button images before removing the
  buttons that referenced them;
- externalized destination mouse arrays were allocated only on the first home
  visit, never freed or resized, and could therefore leak or be overrun after
  destination data changed;
- an empty destination list divided by zero while building its scroll track,
  underflowed its list-window start, and later dereferenced destination zero;
- destination selection narrowed through signed and unsigned eight-bit state,
  while queued drop-down callbacks trusted stale visible slots;
- order-grid regions survived a later empty render and could continue to
  mutate a list that no longer had scrollable rows;
- the shipment page loaded a new order-grid video object on every redraw and
  never released those per-render objects; and
- shipment callbacks manually remapped sparse live rows and initial entry
  selected records beyond the visible shipment-row capacity.

`BobbyRayFulfilmentModel` now owns dependency-free visible counts, stale
window normalization, empty/exact-end selection rules, bounded next/previous
navigation, visible-slot mapping, and sparse-record mapping. The live pages
use those rules at destination refresh, drop-down movement, arrow callbacks,
shipment entry, and shipment-row callbacks. Inventory, pricing, order and
shipment records, PostalService behavior, save layouts, artwork, campaign
policy, and the 82-common/16-variant Laptop partition are unchanged. A
dedicated headless target covers empty, short, oversized, stale, hidden,
exact-end, keyboard, and sparse-row cases; architecture CI pins all owners,
replaceable region sets, scoped shipment grids, test admission, and raw
lifecycle exclusion. The focused analyzer pass over the shared mail-order and
shipment sources plus all three storefront host variants is clean; obsolete
drop-down layout and keyboard-modifier dead stores found during that pass were
removed.

## Remaining walkthrough

The IMP lifecycle, runtime-content, A.I.M., M.E.R.C., Florist/Funeral, and
Insurance ownership slices and both Bobby Ray page clusters are complete. The
remaining audit queue is deliberately grouped into larger reviewable batches:

1. Extend scoped video, surface, button-image, button, and temporary-render
   ownership from the completed A.I.M., M.E.R.C., Florist/Funeral, and
   Insurance and Bobby Ray clusters across every remaining Laptop page,
   beginning with the connected finance, history, and email document/list
   pages.
2. Extend the completed site-cluster mouse-region and re-entry audit across
   the remaining pages, especially empty data and callback-driven mutation.
3. Extend the IMP format-string rule to the remaining non-IMP Laptop pages and
   validate all rendered text buffers before formatting.
4. Audit the remaining Laptop binary readers and writers for exact reads,
   bounded allocation, staged publication, and failure-safe file ownership.
5. Consolidate non-IMP page re-entry and global selection state so cancelled,
   failed, and repeated visits start from a documented state.
6. Verify every remaining fixed array and paginated list against negative,
   exact-end, empty, and stale-selection cases.
7. Audit pointer and iterator lifetimes in the remaining mutable email and
   personnel UI collections after callbacks mutate them.
8. Make finance, history, email, and hire side effects transactional wherever a
   Laptop workflow can fail after partially committing an operation.
9. Run a final domain-wide static-analysis/warning pass, remove superseded
    dead paths, and convert every confirmed finding into a focused regression
    test or an architecture ratchet.

Every batch must include focused tests, all-host compilation, architecture and
compile-guard ratchets, the normal headless suite, and a fresh ASan run before
merge.
