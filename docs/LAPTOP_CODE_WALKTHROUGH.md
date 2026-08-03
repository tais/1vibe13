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

## Remaining walkthrough

The next passes should stay cohesive and prioritize:

1. XML/localization length validation and remaining fixed-buffer string sinks.
2. IMP page lifecycle, generated-character state, and cross-page index
   contracts.
3. `files.cpp`, history, and the remaining compile-time campaign content tail.
4. UI resource handles and mouse-region create/destroy symmetry across every
   Laptop page.

Every batch must include focused tests, all-host compilation, architecture and
compile-guard ratchets, the normal headless suite, and a fresh ASan run before
merge.
