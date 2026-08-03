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

## Remaining walkthrough

The IMP lifecycle, runtime-content, and first complete A.I.M. ownership slice
are complete. The remaining audit queue is deliberately grouped into larger
reviewable batches:

1. Extend scoped video, surface, button-image, button, and temporary-render
   ownership from the completed A.I.M. cluster across every remaining Laptop
   page.
2. Extend the completed A.I.M. mouse-region and re-entry audit across the
   remaining pages, especially empty data and callback-driven mutation.
3. Extend the IMP format-string rule to the remaining non-IMP Laptop pages and
   validate all rendered text buffers before formatting.
4. Audit the remaining Laptop binary readers and writers for exact reads,
   bounded allocation, staged publication, and failure-safe file ownership.
5. Consolidate non-IMP page re-entry and global selection state so cancelled,
   failed, and repeated visits start from a documented state.
6. Verify every remaining fixed array and paginated list against negative,
   exact-end, empty, and stale-selection cases.
7. Audit pointer and iterator lifetimes in mutable email, personnel, shipment,
   insurance, A.I.M., and M.E.R.C. UI collections after callbacks mutate them.
8. Make finance, history, email, and hire side effects transactional wherever a
   Laptop workflow can fail after partially committing an operation.
9. Run a final domain-wide static-analysis/warning pass, remove superseded
    dead paths, and convert every confirmed finding into a focused regression
    test or an architecture ratchet.

Every batch must include focused tests, all-host compilation, architecture and
compile-guard ratchets, the normal headless suite, and a fresh ASan run before
merge.
