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
- the first strict shipping-destination validator accidentally made all four
  map-location fields mandatory, although established base data intentionally
  contains name-only destinations. That made alpha13 enter the error screen
  during startup. Name-only and complete-location records are now accepted,
  while partially supplied location groups still fail transactionally;
- the same strict pass rejected a production campaign-news sentence of 304
  characters even though its legacy destination is a 300-character field.
  Conversion now stages the complete UTF-8 value before explicitly applying
  the established 299-character-plus-terminator bound;
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
the destination on failure. UTF-8 conversion likewise stages before commit and
rejects overflow by default; only the campaign-news reader opts into its
documented fixed-field truncation policy.
Focused data-free tests cover exact capacity, overflow, unterminated buffers,
malformed and out-of-range integers, strict booleans, signed/exact-end indices,
copy failure without mutation, established name-only shipping destinations,
complete locations, and every partial-location combination. The complete
headless harness also parses a production-sized 304-character campaign-news
record through the real VFS/XML adapter and verifies the fixed-field bound.
Architecture CI applies the adapter and transactional-state requirements to
every `Laptop/XML_*.cpp`, rejects the retired conversion/narrowing calls and
mutable parser-mode globals, pins the specific crash boundaries, and admits the
focused target to ASan CI.

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

## Bobby Ray visual-layout closure batch

The Bobby Ray cluster now derives homepage artwork and sign hitboxes, all five
catalogue canvases and item rows, mail-order controls and scrolling, and the
thirteen visible shipment rows from the dependency-free `BobbyRayLayout`
model. Drawing, mouse regions, scrolling, and invalidation therefore consume
the same authored rectangles and sequences instead of maintaining parallel
macros and mutable coordinate arrays.

This closes two concrete visual risks left after the ownership work: the home
page's twenty-value sign-region array could drift away from the five drawn
signs, and shipping-speed lights were invalidated without the Laptop web-page
vertical offset used to draw them. The shared layout retires both paths while
preserving the existing artwork, localized text, inventory, pricing, order and
shipment records, campaign policy, and save formats. A data-free headless test
pins exact default and shifted-screen geometry, containment, row capacities,
and drawing/input pairings; architecture and AddressSanitizer CI ratchets keep
every Bobby Ray host on that boundary.

## Finance and history ledger ownership and persistence batch

The twelfth cohesive batch converts the connected finance and history record
pages together. Both now stage graphics, buttons, and button images through
`LaptopPageResourceOwner`; history owns its title and divider graphics instead
of borrowing finance's numeric handles. `LaptopRecordPageModel` supplies the
shared dependency-free rules for record layouts, page counts, stale saved-page
normalization, checked offsets, exact-end page sizes, signed balance changes,
saturating summaries, and legacy-adjusted day buckets. `LaptopRecordFile`
provides scoped file handles and exact read/write adapters above FileMan.

Confirmed faults fixed by this batch include:

- partial entry leaked resources, and button creation could publish invalid
  numeric handles after an intermediate failure;
- history rendered through finance-owned numeric video handles, coupling two
  otherwise independent page lifetimes;
- stale persisted page numbers selected blank pages, while next-button state
  destructively loaded another page merely to discover whether it existed;
- short or partially written ledger records left fields uninitialized and
  then published them into linked lists;
- otherwise complete but malformed records could index profile and town arrays
  with sentinel or exact-end IDs, interpret quest text as a format string, or
  leave the first unknown finance transaction's render buffer uninitialized;
- finance rewrote its balance with a create-always open before appending a
  transaction; with the corrected FileMan disposition semantics that erased
  the existing transaction ledger;
- the replacement create-or-open transaction path rejected a brand-new
  zero-byte finance file before it could write the balance header, so the
  starting-cash transaction asserted while creating every new campaign;
- finance balance addition, projections, daily summaries, display magnitudes,
  and early-campaign day arithmetic could overflow or underflow;
- history allocation failures were dereferenced, append ignored its argument,
  and several early returns leaked an open file; and
- the unused history page writer added a nonexistent four-byte header to its
  seek offset, so invoking it would overwrite bytes in the following record.

Finance now validates the complete header-plus-record layout, writes the
balance and appended record through one checked persistence operation, and
publishes campaign balance, profile cost, statistics, and UI state only after
the record is durable. The only incomplete layout accepted is the zero-byte
file just created for a first transaction; a FileMan/VFS headless regression
test executes the starting-cash path and verifies that a later transaction
appends without truncation. History validates its headerless record layout,
checks
every field read/write, rejects malformed tails, assigns persisted IDs, and
reloads only a normalized live page after an append. Its active full-ledger
update writes from byte zero; the superseded corrupt page writer and all
destructive page probes are gone. Both linked-list builders reject allocation
failure and clear partial state, and finance/profile totals clamp rather than
executing signed overflow or unbounded profile indexing.

The existing finance and history file layouts, transaction and history codes,
quest behavior, artwork, save fields, XML/Lua interfaces, campaign content
policy, and 82-common/16-variant Laptop partition are unchanged. A dedicated
headless target covers malformed layouts, empty/exact/stale pages, offset
overflow, signed balance and summary overflow, and legacy day adjustment.
Architecture CI pins both owners, exact-I/O integration, safe navigation, test
admission, and removal of the superseded paths.

## Email ownership, mutable inbox, and persistence batch

The thirteenth cohesive batch treats the inbox, opened-message viewer, new-mail
notification, and delete confirmation as one ownership domain. Four independent
`LaptopPageResourceOwner` transactions cover the base page and each transient
mode, so a failed image, button, or region acquisition rolls back without
publishing a half-entered screen and a later entry can retry. The
dependency-free `LaptopEmailListModel` supplies inbox and body page counts,
stale-page normalization, signed/exact-end index validation, persisted-width
checks, bounded fixed-buffer text operations, and deterministic unread-message
ordering.

Confirmed faults fixed by this batch include:

- partial page or modal entry leaked graphics and input registrations, while
  teardown could remove buttons after their backing images;
- new-mail rendering shadowed the persisted modal-state flag with a separate
  function-static value, while two other page/button-state flags were write-only;
- insertion published a message before rebuilding its page list, deletion
  destroyed the live node before allocation could succeed, and save loading
  discarded the current inbox before validating its replacement;
- malformed subject sizes and IDs could allocate or index outside the fixed
  legacy contracts, and email metadata restored version but not type;
- the newest-unread command selected the oldest message, sorting exchanged
  only part of a message payload, and stale saved page/row state could select a
  missing page or exact-end message;
- Wildfire subject selection could underflow an eight-bit record number, while
  profile-backed sender and IMP-result paths accepted invalid profile indices;
- an oversized single body record could prevent page construction from making
  progress, and the body-page table could overrun its fixed sentinel slot;
- several localized or saved strings were copied or appended without a bound,
  and runtime email text could be interpreted as a variadic format string; and
- the merc-available body path requested more than 170 records even though it
  only selected one of two bodies.

Message insertion, removal, explicit page rebuilding, and save replacement now
stage a complete page topology before publishing it. Save/load uses scoped
exact I/O, validates counts, IDs, subject alignment and termination, rejects
duplicates, restores both metadata fields, and keeps the live inbox untouched
on failure. Viewer preprocessing bounds profile and record access, normalizes
body pages, and forces progress for a record taller than one page. Sorting now
moves the complete persisted and presentation payload together, and the
newest-unread choice is stable by date then message ID.

The existing `Email.edt`/`Email25.edt`, XML identifiers, sender bytes,
substitution tags, fixed save structures, campaign policy, artwork, and
82-common/16-variant Laptop partition are unchanged. A dedicated headless
target covers empty, stale, exact-end, overflow, malformed-size, fixed-buffer,
body-page, Wildfire-offset, and unread-order cases. Architecture CI pins the
four owners, staged list/page publication, exact persistence, safe rendering,
test admission, and removal of the superseded resource and list paths. The
focused analyzer pass over `email.cpp` is clean; the adjoining save/load pass
reports no finding in the email persistence region.

## Personnel roster, resource ownership, and text-safety batch

The fourteenth cohesive batch treats Personnel's live roster, three persisted
departed categories, inventory window, ATM controls, and popup regions as one
mutable page domain. The dependency-free `PersonnelRosterModel` owns selection
and pagination rules, departed-list normalization and transactional category
changes, inventory-window/slider arithmetic, index validation, saturating
currency conversion, and bounded fixed-buffer text helpers.

Confirmed faults fixed by this batch include:

- page entry assumed actor slot zero existed and several later statistics and
  cost loops repeated that assumption instead of using the captured player
  roster;
- current and departed selection used six partially synchronized integer
  globals, allowing stale, exact-end, and partial-last-page selections;
- departed rendering walked three fixed save arrays with unbounded searches,
  trusted corrupt profile IDs, duplicated profiles across categories, and
  could partially mutate the lists when a destination was full;
- the inventory scrollbar divided by zero for exactly eight visible items and
  produced invalid positions for empty, short, and stale windows;
- inventory rendering trusted saved item IDs, interface-graphic frames,
  weapon/magazine class indices, and object presence before dereferencing;
- Personnel initialization overwrote the title coordinate with an unrelated
  data coordinate, displacing the page heading;
- main, departed, inventory, ATM, tooltip, portrait, and transient-overlay
  resources used independent open-coded creation flags and teardown orders,
  leaving partial-entry leaks and stale callback registrations possible;
- carried-cash and team-cost totals could overflow signed display values;
- achievement and help-text paths trusted profile and personality-table
  indices, excluded valid profile zero, and built popup strings through about
  300 unbounded concatenations; one multi-disability path also formatted a
  buffer using that same buffer as an input; and
- localized and runtime strings were passed directly to variadic rendering or
  unbounded formatting functions.

The live roster is now a bounded `std::vector<SoldierID>` snapshot and both
views use `RosterCursor`. Departed arrays retain their established fixed save
layout and `-1` sentinel, while views filter invalid IDs and duplicates and
category moves stage all three arrays before publication. Every page resource
set uses `LaptopPageResourceOwner`; temporary video objects use scoped handles.
All Personnel fixed-buffer formatting is bounded, concatenation always
terminates, and rendered data uses an explicit format.

The save format, profile IDs, departed array order, artwork, localized text,
finance/history records, and campaign behavior are unchanged. Focused headless
tests cover empty/stale/exact-end rosters, partial pages, duplicate and corrupt
departed entries, full-destination rollback, inventory window and exact-page
slider behavior, signed currency saturation, and text truncation. Architecture
and ASan ratchets pin the model, owners, retired-state removal, safe resource
and text paths, and focused test admission.

## Campaign History ownership, report navigation, and persistence batch

The fifteenth cohesive batch treats Campaign History's landing, summary,
incident-detail, and news pages together with the mutable campaign/incident
ledger. The dependency-free `CampaignHistoryModel` owns incident-page
normalization and wrapping, configured report retention, exact-transfer checks,
saturating arithmetic, picture-frame selection, direction composition, and
bounded fixed-buffer copies. `CampaignHistoryText` is the legacy-facing bounded
format adapter for localized and generated report text.

Confirmed faults fixed by this batch include:

- failed page entry leaked previously loaded graphics and registered regions,
  while the incident page aliased every failed picture-library handle to the
  event image and later deleted that shared handle once per failed library;
- empty incident histories underflowed previous-page navigation, stale page
  state and the serialized incident count could index beyond the live vector,
  and an oversized configured report limit underflowed the load-retention
  threshold and discarded every loaded report;
- empty picture libraries caused modulo by zero, event and ordinary picture
  frames were not checked against their loaded object counts, and missing
  artwork could be dereferenced instead of rendering the report without art;
- the News entry path rendered the Summary page, and invalid landing-page text
  IDs copied a fallback but then continued into an out-of-range localization
  lookup;
- incident and campaign saves accepted short writes, while short loads
  published partially overwritten records or destroyed the existing live
  ledger before the complete replacement had been validated;
- incident filler and implicit alignment bytes and part of the campaign
  aggregate could remain uninitialized, while counters, money, consumption,
  incident IDs, and multi-category display sums could wrap, overflow, or
  retain NaN;
- unknown enemy and militia classes were attributed to the mercenary group,
  and unknown statistic types fell through into the promotion counter; and
- report construction used more than 130 unbounded wide-string operations and
  two rotating global direction buffers, making nested formatting truncate,
  alias, or overwrite earlier text.

All four pages now stage shared and page-specific resources through
`LaptopPageResourceOwner` and publish only a complete acquisition. Failed
optional picture libraries simply have no independently owned handle; reports
remain text-renderable. Every page derives bounds from the live incident
vector. Campaign and incident loads retain the previous object until every
expected byte and retained record has loaded, while saves require exact byte
counts. Aggregate counters and IDs saturate, floating totals remain finite,
and direction text is returned as an owned `std::wstring`.

The established incident POD field order and widths, campaign header order,
configured tail-retention behavior, artwork names, localized strings, XML,
Lua, and campaign behavior are unchanged. Focused headless tests cover
negative/exact-end/stale/empty navigation, short and oversized report limits,
integer and floating-point saturation, short transfers, empty picture
libraries, all direction combinations, and unterminated/truncated fixed text.
Architecture and ASan ratchets pin the model, ownership and persistence paths,
bounded text, retired duplicate helpers, and focused test admission. The
targeted path-sensitive analyzer is clean for all three production translation
units changed by the batch.

## Shared shell, widgets, residual pages, and dormant-path closure

The sixteenth and final scheduled batch closes the audit queue across the
Laptop shell, shared widgets, IMP pages, remaining service pages, briefing and
Encyclopedia code, and the residual file/list boundaries. The dependency-free
`LaptopUiStateModel` now supplies the common signed/exact-end index,
pagination, scrolling-window, adjacent-selection, sentinel-list, bounded-text,
and exact-transfer rules instead of leaving each page to reproduce them.

Confirmed faults fixed by this batch include:

- shell bookmarks, notification regions, program buttons, title bars,
  minimize/error overlays, and desktop graphics could partially publish or
  tear down resources in an unsafe order after a failed or repeated entry;
- the shared table, drop-down, and dialogue widgets accepted stale selections,
  exposed hidden rows, divided or indexed through empty data, and mixed
  callback-replaced regions with longer-lived page resources;
- IMP pages shared raw numeric video handles across transitions, so one page
  could unload another page's graphics, while numerous localized/profile/item
  strings were copied or formatted without a destination bound;
- Files allocated only two bytes for a wide-string terminator, overflowing on
  platforms with four-byte `CHAR16`, and its saved/list text and terrorist face
  paths trusted allocation, profile, and buffer assumptions;
- briefing, facility, intel, militia, PMC, WHO, postal, and comparison pages
  leaked partial entry resources or transient graphics, retained stale
  callback state, and trusted exact-end records, matrix slots, town/profile,
  item, preference, and squad indices;
- Merc Compare leaked its first transient face on every comparison render;
  the Briefing Room leaked each map image it rendered and could navigate from
  invalid locations or pages; and
- the disabled Encyclopedia implementation no longer compiled when enabled
  and contained an unreachable SAM-site match, a sector-Z typo, unsigned
  reverse traversal, unchecked profile/item/quest access, uninitialized image
  paths and quest text, null button-slot dereferences, partial-entry leaks, and
  exit functions that always reported failure.

Shell and page entry now stage complete `LaptopPageResourceOwner`
transactions. IMP retains its numeric compatibility surface through one
per-page facade, while dynamic artwork uses move-only handles. Callback-driven
widget/table/matrix regions have independent replaceable owners. Remaining
fixed buffers use terminating bounded helpers or size-aware formatting, binary
transfers require their exact legacy sizes, and mutable selections derive
bounds from current containers rather than persisted counters.

The established save layouts, page IDs, resource filenames, localized text,
XML/Lua data, profile/item identities, and campaign behavior are unchanged.
Focused headless tests cover all shared model boundaries and independently
removable resource handles. The dormant Encyclopedia/Briefing implementation
is compiled with `ENCYCLOPEDIA_WORKS` in every host configuration. Architecture,
compile-guard, string-sink, all-host, headless, and ASan checks protect the
closure.

## Closure status

All 98 Laptop translation units have now received the structural walkthrough,
and every scheduled ownership, input-boundary, persistence, navigation,
bounded-text, and static-analysis batch in this document is complete. There is
no remaining Laptop audit batch on the architecture agenda. This is a domain
closure milestone, not a claim that future gameplay or data-dependent defects
are impossible; newly discovered defects should be handled as focused normal
maintenance with regression tests and, where the rule is architectural, a
ratchet here and in `CheckArchitectureBoundaries.cmake`.

## Post-closure IMP and encrypted-text hardening

Post-closure validation found two defects that crossed the original batch
boundaries. Six IMP trait/background paths derived button images with
`UseLoadedButtonImage`, but the per-page facade did not adopt those derived
handles, so page exit could not release them. In addition, a failed page
acquisition cleared the owner without latching failure; later setup could
publish a new partial resource set and feed `-1` button IDs into compatibility
APIs or direct button-slot access. Derived images now enter the same owner,
failure remains latched until the next page begins, clicked-state access is
checked, and the character-profile render/input dispatcher suppresses a failed
page while still allowing a subsequent transition to reset the transaction.

The IMP text renderer also called `wcslen` on an uninitialized buffer when an
encrypted EDT record could not be opened or read. The shared encrypted-record
reader now clears destinations before I/O, requires exact byte counts, rejects
invalid record widths, reserves the final in-bounds character as a terminator,
and samples from only a non-empty complete-record range. IMP renders text only
after a successful load. The SGP button compatibility entry points used during
page setup reject negative, exact-end, deleted, and invalid derived-image
handles instead of relying on assertions before indexing global registries.
Focused model tests cover the failure latch; real headless FileMan/button tests
cover missing, truncated, unterminated, and invalid-handle paths. Architecture
ratchets preserve the complete failure chain.

## Post-closure Laptop visual-layout organization

The A.I.M. member page previously encoded its fixed-pixel artwork geometry in
more than 200 preprocessor definitions. The legacy and expanded-equipment
variants each carried a parallel coordinate family, and rendering, mouse
regions, invalidation, navigation controls, equipment cells, and help text
selected those families independently. The two variants also duplicated the
complete stat and portrait-status renderers. That made a visual adjustment
easy to apply to only one drawing path or to omit from the matching hitbox.
The stat dot leaders were also fixed to the original 640-pixel screen origin,
so unlike the surrounding panel they did not follow a centered widescreen
offset.

The dependency-free `LaptopLayoutModel` now supplies the common point,
rectangle, text-area, containment, and overlap vocabulary. On top of it,
`AimMemberProfileLayoutModel` derives both profile variants from one explicit
set of screen and web-canvas anchors. Typed geometry describes the portrait,
stats, pricing, biography, equipment grid, kit selector, help text, and
navigation controls. Entry-time mouse regions and run-time rendering consume
the same selected `Layout`; equipment slots use one tested row-major cell
mapping; and shared behavior is rendered once.

`AimWebsiteLayoutModel` finishes the rest of the coupled A.I.M. geometry. The
default logo rectangle is shared by drawing and its link hitbox; rust tiles use
one row-major canvas; and the stateful video conference derives its terminal,
portrait, contract/equipment/contact/authorization controls, selection lights,
message popup, and animated title frames from one layout. Facial Index drawing,
hitboxes, nicknames, status overlays, help text, and keyboard/mouse pagination
share one 8-by-5 grid and bounded page model. Policies likewise uses the same
sequences for its table-of-contents drawing and regions and for its navigation
and agreement buttons.

That consolidation exposed two real Facial Index hazards. Activating the page
button destroys and recreates its own `GUI_BUTTON`; the callback now returns
immediately instead of invalidating through that retired pointer. Loops over
the configurable 255-profile range now use `std::size_t` instead of an 8-bit
counter that could wrap forever. Stale page starts normalize safely and a
partial final page creates and renders only live profile slots.

The same layout boundary now covers the remaining cross-consumer service-site
geometry. `MercFilesLayoutModel` gives the M.E.R.C. Files equipment artwork,
backgrounds, count text, tooltip regions, and five kit buttons one tested
7-by-3 grid and stride. `FloristSiteModel` gives card drawing and hitboxes one
3-by-3 grid and gives flower buttons, titles, prices, and descriptions one row
sequence. All of these models follow the centered Laptop screen/web anchors.

The artwork, authored pixel positions, localized text, equipment capacity,
profile data, and campaign behavior are unchanged. Remaining fixed constants
describe page-specific artwork or text flow; geometry consumed by more than one
drawing, input, invalidation, or control path belongs in a typed layout. Focused
dependency-free tests pin exact legacy and expanded coordinates, containment,
non-overlap, row/column mapping, pagination, animation interpolation, layout
rhythm, and centered-screen translation. Architecture and ASan ratchets
preserve the models, their production integrations, removal of the parallel
macro families, and focused test admission.

The same A.I.M. boundary now covers Sort and Archives. Sort previously
populated a flat 30-element coordinate array during page entry and recovered
each light as an unchecked `mode * 2` pair. Fifteen drawing/invalidation paths
and fifteen individually repeated callbacks could drift from the matching
localized-text hitboxes. `AimWebsiteLayoutModel::SortLayout` now derives the
panel, thirteen criterion controls, two order controls, three navigation
tiles, headings, labels, regions, and invalidation rectangles from one set of
anchors. Table-driven regions and callbacks replace the parallel functions;
invalid modes are ignored, and unusually long localized labels remain bounded
to the authored panel instead of underflowing or overflowing mouse geometry.

Archives drawing and input now share one tested 5-by-4 portrait grid and one
section-based popup layout. Portrait frames, faces, nicknames, face regions,
page controls, shadows, text, and the dynamic Done artwork/region all derive
from `ArchiveLayout`. The migration also fixes sparse XML archive pages: page
availability previously inspected only profiles 0, 20, 40, and 60, hiding a
page when its first slot was empty even if later slots were populated. Page
discovery scans each bounded page, stale selections normalize to a visible
page, and cycling skips disabled pages without the old signed `idPage` state.
The unused bottom-button render resource and no-op page-button machinery are
retired.

Focused dependency-free tests pin exact and centered-screen coordinates,
drawing/input pairing, grid containment, localized hitbox bounds, invalid
control/profile indices, partial final pages, popup growth, first-slot gaps,
and sparse-page wrapping. Architecture and ASan admission ratchets preserve
the layouts, integrations, retired macro/array families, and regression cases.
