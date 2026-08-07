# Utils code walkthrough

Status: active architectural refactor, 7 August 2026.

## Scope and completion criteria

`Utils` contains 36 production translation units used by every application
host. This walkthrough tracks them explicitly so a successful local fix is not
mistaken for completion of the directory-wide audit. Five coherent batches now
cover shared interactive UI, text/localization, media lifecycle, indexed XML
infrastructure, and the encrypted text-record boundary. The tactical LBE popup
XML loader is included because it is the persistence boundary for the
popup-definition graph.

A batch is complete only when it has:

- explicit ownership and idempotent teardown for heap and engine resources;
- negative, exact-end, capacity, null, and partial-initialization checks before
  legacy array or handle access;
- transactional publication where a multi-step load can fail;
- dependency-free model tests for reusable state rules and real-engine
  headless tests for legacy ownership paths;
- Release, AddressSanitizer, and architecture-boundary CI coverage.

## Interactive UI ownership batch

| Owner | Faults found | Enforced state |
| --- | --- | --- |
| `popup_class.cpp` | Heap-owned callback ambiguity, label APIs that mixed heap ownership with addresses of temporary strings, leaked callback-index nodes, fixed region-capacity overflow, and use-after-free when a callback deletes its popup | Callbacks use unique ownership, labels pass by constant reference, the bounded ID directory owns value mappings, region capacity is checked, and post-callback work re-resolves the popup ID |
| `popup_definition.cpp` | Shallow pointer graphs, rejected-content leaks, unchecked generator IDs, and actor lookup before selection validation | Definitions deep-copy owned content, failed additions retain staging ownership, generators are bounded, and application validates selection before resolving an actor |
| `XML_LBEPocketPopup.cpp` | Partially published parses, unchecked nesting and pocket IDs, dangling submenu staging, and unknown generators | A complete temporary definition map is published only after successful parsing; all depth, ID, parent, and generator transitions are checked |
| `PopUpBox.cpp` | Header-defined registry state, exact-end access, partial string allocation, incomplete reset, stale current handles, and a fixed 100-character render copy | One implementation-owned registry validates every handle and text coordinate; strings publish transactionally; full reset and removal are idempotent; rendering uses owned text directly |
| `Animated ProgressBar.cpp` | Public raw registry exposure, invalid geometry and IDs, replacement leaks, out-of-range percentages, and truncated or uninitialized loading-hint IDs | The registry is private and checked, replacement is transactional, percentages clamp, and 16-bit bounded hint selection handles an empty candidate set |
| `Slider.cpp` | Zero-extent division, unclamped values, uninitialized thumb dimensions, stale current pointers, and partial resource initialization | Dependency-free slider math handles zero and exact endpoints; initialization, removal, callback lookup, and image ownership are guarded and idempotent |
| `MercTextBox.cpp` | Arbitrary IDs, partially published icons/images, leaked replacement surfaces, teardown gaps, and a 16-bit fill-loop counter that wrapped on the normal 350x207 surface | IDs and dimensions are bounded; multi-resource loads stage before publication; every box/surface/icon is released; the fill area uses a non-wrapping count |
| `Text Input.cpp` | Nested modes restored a freed tail/active pointer, empty levels could not be represented safely, field creation leaked on failure, several getters/setters overran or dereferenced invalid fields, and inactive rendering could loop forever | Stack frames retain head/tail/active/color ownership, allocation and removal are rollback-safe, text operations are bounded and terminated, callbacks validate live fields, and traversal always advances |
| `WordWrap.cpp` | Node allocation failures leaked partial lists and callers dereferenced failed string allocations | One append helper owns transactional node/string creation and every failure releases the already-built list |

The dependency-free `UtilsUiStateModel.h` centralizes signed/exact-end index
validation, slider conversion/clamping, and bounded ID-directory behavior. It
does not depend on SGP, rendering, game data, or a running application.

## Verification owned by this batch

- `utils_ui_state_model_tests` covers negative and exact-end indices,
  zero-extent slider math, rounding and clamping, duplicate callback IDs,
  full directories, stale IDs, removal, and teardown.
- `ja2_headless_tests` covers callback replacement/destruction, deep popup
  definition copies, popup-box allocation/text/reset, progress-bar invalid and
  replacement paths, rejection of the retired pointer-string popup API, and
  populated plus empty nested text-input modes.
- `CheckArchitectureBoundaries.cmake` keeps the model, unique ownership,
  private registries, transactional XML publication, bounded formatting,
  tests, documentation, and ASan target in place.

## Text and localization safety batch

| Owner | Faults found | Enforced state |
| --- | --- | --- |
| `message.cpp` | Platform-dependent unbounded variadic formatting, allocation-unsafe string creation, live-list mutation during partial save reads, uninitialized persisted padding, and unbounded last-quote concatenation | One counted compatibility formatter rejects truncation, string replacement publishes only allocated storage, all 256 saved entries stage before publication, persisted metadata is zero-initialized, and quote assembly is bounded |
| `LocalizedStrings.cpp` | A reference to the common-file state was assigned the section state, so the real section was never marked loaded; section association always reported failure; failed loads were cached; arbitrary topics created map entries | Fixed topic arrays reject invalid values, common and section states remain distinct, associations report accurately, and only successful loads enter the retryable cache |
| `Text Utils.cpp` | Item text indexed the global item table without validating loaded capacity, null enum tables crashed, command-line parsing dereferenced null control arguments, and UTF conversion depended on deprecated facets | Item reads reject exact-end indices and clear outputs, enum/parser boundaries validate inputs, and VFS's maintained UTF conversion owns encoding translation |
| `Font Control.cpp` | Invalid or unloaded font IDs reached assertion-only APIs and absent shade tables were dereferenced | Font, shade, palette, and wrapper inputs are checked before accessing the font registry |
| `ImportStrings.cpp` | Startup performed three unused localization reads and generated three-digit dialogue names through floating-point arithmetic | Import registers only required files and formats dialogue paths deterministically with integer stream width |
| `Multilingual Text Code Generator.cpp` | Working-directory composition was unbounded and every failure after changing directory leaked process state | Path construction is counted and a scope guard restores the caller's directory on success and failure |

`TextInfrastructureModel.h` owns the dependency-free signed index, bounded
copy/append, serialized-size, and retryable lazy-load rules. The adjacent SGP
compatibility formatter and multiplayer chat consumer use the same explicit
capacity contract, so Windows and POSIX hosts no longer diverge at variadic
text sinks.

Verification for this batch includes `utils_text_state_model_tests` for null,
negative, exact-end, exact-capacity, truncation, serialized-size, and failed-
then-retried load state. `platform_legacy_tests` executes counted legacy
`%s`/`%S` formatting and termination. `ja2_headless_tests` exercises atomic
message replacement and proves a truncated message block cannot mutate live
save state. All three run in the normal and AddressSanitizer matrices.

## Media lifecycle batch

| Owner | Faults found | Enforced state |
| --- | --- | --- |
| `Cinematics.cpp` | Decoder backing memory was declared for destruction before its borrowing handle, null filenames reached `FileOpen`, file/decoder state published incrementally, unchecked dimensions and frame timing narrowed into fixed types, blit arithmetic overflowed signed integers, arbitrary opaque pointers were dereferenced, PCM byte counts narrowed to SDL's `int`, failed queue/decode operations left playback stuck, and loop audio clocks retained the prior pass | File, source bytes, decoder, and SDL stream have ordered move-only ownership; a complete staged flic publishes only after metadata validation; owned pointers, clipped geometry, clock regression, PCM format/length/accounting, queue failure, decoder failure, loop reset, initialization, close, and shutdown are bounded and deterministic |
| `Cinematics Bink.cpp` | The compatibility surface had no direct regression coverage, so future placeholder changes could accidentally imply support or dereference caller input | The intentionally unsupported `.BIK` path remains an explicit null/false, input-agnostic, idempotent stub while shipped `.SMK` playback remains unaffected |
| `Music Control.cpp` | Public vectors exposed manually allocated filename pointers, initialization mutated live lists incrementally, invalid list/mode values reached arrays, failed playback left mixed flags, natural completion still reported playing, negative fade speeds reversed transitions, and a deferred callback from a stopped track could retire its replacement | Private `std::string` lists publish transactionally and expose checked read-only accessors; playback has one epoch token, callbacks accept only the active incarnation, stop/shutdown invalidate pending completion, and mode/index/fade transitions are bounded |
| `Sound Control.cpp` | Exact-end sample/ambient IDs and null filenames reached fixed arrays or `strstr`, soldier playback applied master volume twice, weapon amplification could wrap, delayed playback retained invalid IDs across shutdown, positional APIs accepted arbitrary slots/sample IDs, deleting the last entry retained a stale high-water mark, new active entries never started, failed starts became non-retryable, and attenuation could overflow or divide by a zero viewport | Every gateway validates before access, volume/pan/amplification saturate, delayed state and positional ownership reset idempotently, fixed slots recount to zero and remain reusable, new/failing playback publishes a retryable state, dead handles retire, and distance math uses bounded doubles |

`MediaLifecycleModel.h` owns the dependency-free signed-index, volume, callback-
epoch, clipped-blit, audio-format/queue, monotonic-duration, positional-prefix,
and attenuation rules. It has no SDL, libsmacker, SGP, tactical, or application
dependency; the legacy adapters consume it at their unsafe boundaries.

Verification includes `utils_media_lifecycle_model_tests` for negative/exact-
end indices, saturated volume, epoch cancellation/wrap, edge clipping, malformed
audio metadata and queue sizes, regressed clocks, full deletion, and zero-size
viewports. `platform_legacy_tests` executes the real public adapters with null,
empty, foreign, exact-end, full-capacity, restart, replacement-callback, and
repeated-shutdown cases. Both run in the Release and AddressSanitizer matrices,
while every application host compiles the production changes.

## Encrypted text-record boundary

`Encrypted File.cpp` is the shared reader for Laptop's legacy EDT catalogs and
other fixed-record text consumers. The reader previously accepted short reads,
left destinations stale or uninitialized on open/seek/read failure, trusted an
authored terminator that malformed records could omit, underflowed the random
record range for undersized files, and excluded the final complete record from
selection. Exact reads now stage in temporary 16-bit storage, failure always
leaves an empty destination, the last in-bounds character is terminated after
decode, invalid/empty record geometry is rejected, and random selection spans
all complete records. Null decode input is a safe no-op.

`ja2_headless_tests` exercises the real VFS/FileMan path for complete,
unterminated, truncated, and missing records as well as destination clearing.
Laptop's IMP renderer separately gates display on reader success, while the
architecture boundary preserves the reader contract and tests in every host
and AddressSanitizer build.

## Data persistence foundation

| Owner | Faults found | Enforced state |
| --- | --- | --- |
| `INIReader.cpp` | INI overlays mutated live properties while parsing, merged-file discovery could report a file that no profile contained, scalar and list conversions accepted trailing or overflowing input, list failure published a valid prefix plus the fallback, zero-capacity string copies underflowed, null destinations were dereferenced, and the legacy pointer overload leaked every result | Each overlay parses into a copy and publishes only on success; both constructors report a merged file only after a profile supplies a valid layer; numeric tokens are complete, finite, and range checked; lists publish whole or become the established single `[0]` fallback; every copy has an explicit capacity including zero; and the compatibility pointer borrows reader-owned storage until the next legacy call |
| `XMLProperties.cpp` | XML callbacks published keys before document completion, short reads were treated as complete buffers, manual file release leaked open state on exceptions, and missing identifiers could populate empty sections or keys | Existing properties seed a staged overlay; reads must be exact; malformed, truncated, or structurally incomplete documents leave live properties untouched; required identifiers are validated; and balanced unknown subtrees remain forward-compatible and ignored |
| `XMLWriter.cpp` | Attribute strings bypassed escaping, numeric output followed the process locale, comments could emit forbidden `--`, invalid controls reached output, short writes reported success, and incomplete or already-failed node/attribute state could be persisted | Text and attributes share XML escaping, numbers use the locale-independent XML representation, comments are made structurally valid, invalid controls and failed close operations poison the staged document, writes succeed only for a complete document and exact byte count, and all existing tag names, paths, and schemas remain unchanged |

`DataBoundaryModel.h` owns dependency-free, locale-independent strict
scalar/list conversion,
bounded copy, transactional publication, unknown-subtree depth, XML escaping,
comment, and exact-transfer rules. `utils_data_boundary_model_tests` covers
overflow, trailing input, non-finite values, partial-list rollback, null and
zero-capacity destinations, staged publication, nested unknown elements,
reserved characters, illegal controls/comments, and short transfers.
`ja2_headless_tests` exercises the real PropertyContainer/VFS adapters for
overlay preservation, unknown-subtree compatibility, truncated and missing-
attribute rollback, and escaped writer/reader round trips.

## Indexed localization XML boundary

| Owner | Faults found | Enforced state |
| --- | --- | --- |
| `XML_Language.cpp` | Parser mode and destination lived in process globals, records wrote directly into two live tables before the document completed, unchecked unsigned conversion admitted negative/partial/overflowing indices, exact-end indices reached fixed arrays, and UTF-8 conversion silently truncated oversized messages | Parser mode and destinations are per-load state; strict C-style unsigned indices and converted text capacity are validated; sparse records stage in document order; and metadata plus tactical messages publish only after complete parse success, with localized overlays changing text but not base metadata |
| `XML_SenderNameList.cpp` | A process-global localization flag selected parser behavior, unchecked narrowing accepted invalid indices, names silently truncated, and each record mutated the live sender table before later syntax or content failures | Parser state is local; decimal indices and converted name capacity are checked; sparse and duplicate records stage in document order; and the sender table changes only after the whole document succeeds |

`IndexedXmlModel.h` owns the dependency-free ASCII trimming, strict bounded
numeric parsing, terminator-aware text-capacity checks, ordered staging, and
explicit publication rules. Ordered staging intentionally preserves the legacy
duplicate-last-wins result, while publishing only named indices preserves base
tables beneath sparse localization overlays. Unknown XML subtrees, established
resource paths and schemas, and localized missing-file success remain unchanged.

Verification includes `utils_indexed_xml_model_tests` for 999/1000 and 499/500
boundaries, negative/overflowing/partial numeric input, decimal and legacy
C-style syntax, zero/exact text capacities, sparse records, duplicates, and
publication timing. `platform_legacy_tests` exercises both production loaders
through the real VFS/Expat adapter and proves malformed, exact-end, and
oversized documents cannot partially replace live text or metadata. Both tests
run in the normal and AddressSanitizer matrices.

## Item XML staging model and adapter — data/XML Slices 3–4 of 5

`ItemDataStagingModel.h` defines the dependency-free transaction and the legacy
`XML_Items.cpp` Expat adapter now consumes it for both required base files and
optional localized overlays. Slice 3 established the bounded ownership and
publication contract; Slice 4 moves the production reader behind that contract
without changing resource paths or the XML schema:

- `RequiredBaseLoadTransaction` owns exactly one fresh default item candidate,
  but snapshots only the compact live `StoreInventory` and `WeaponROF` values. It
  never requires a second dynamic `INVTYPE` table beside the legacy static
  `Item[]`. This matches the legacy reload rule: the `<ITEMLIST>` root clears
  `Item[]`, while an omitted `BR_NewInventory`, `BR_UsedInventory`, or `BR_ROF`
  field preserves its previous auxiliary value.
- Base records may be sparse. At a duplicate index, the last record with a
  nonzero item class wins the `INVTYPE` payload; a later zero-class record does
  not erase that item. Authored auxiliary fields merge independently in file
  order, regardless of item class, so each field's last authored value wins
  while omitted fields retain their prior live or earlier-record value. This is
  the precise legacy callback behavior.
  Records at or above the compiled capacity are ignored before access, as they
  were by the legacy adapter. `maxItemsRead` is the exclusive high-water mark
  of every accepted nonzero-class record, rather than the last record in file
  order; this intentionally hardens valid unsorted mod files.
- Every `<ITEM>` starts from a new value-initialized `INVTYPE`, auxiliary patch,
  and localized presence patch. The callbacks stage records only; they never
  clear or update `Item[]`, `StoreInventory`, `WeaponROF`, or
  `gMAXITEMS_READ`. The adapter also requires a complete `<ITEMLIST>` root and
  parses indices and auxiliary values with checked narrowing before accepting a
  record. A valid out-of-range record is ignored at staging, but a syntactically
  or semantically invalid checked field poisons the whole document even when
  that record's index is out of range; bounds do not excuse malformed input.
- `OptionalLocalizedLoadTransaction` owns field-presence patches and no item
  candidate. It first validates every final patch without writes, then applies the validated set
  directly through an exact-`void`, `noexcept` publisher. It changes only
  authored text, never clears an omitted field, auxiliary table, or loaded-item
  bound, and a duplicate index replaces the earlier whole patch. This removes
  the accidental dependency on text left in the previous parser record. The
  transaction types encode resource policy: `resourceMissing()` always fails a
  base load and always makes an optional localization load a successful no-op,
  so an adapter cannot pass the wrong runtime requirement.
- The dependency-free model can represent missing indices, strict integer
  parse/narrowing failures, over-capacity character data, malformed XML, and
  truncated XML as transaction failures. The Slice 4 production adapter
  currently wires missing indices, checked index/auxiliary numeric failures,
  Expat parse failures, truncation, and localized overlay rejection into that
  rollback contract. Its legacy fixed character accumulator can still truncate
  text, and UTF conversion success is not yet checked; production rollback for
  character overflow and invalid UTF is explicitly deferred to Slice 5.
  Neither base nor localized state publishes until the complete document and
  every validation implemented at this boundary succeeds. The only mutating
  boundary is a prevalidated no-fail publisher. Publication is logically atomic
  under the existing single-threaded startup contract: validation and
  conversion finish before the first live write, and no readers or re-entrant
  reloads run during the fixed-array copy. The failure classes wired in Slice 4
  therefore retain every live table without allocating a rollback copy of
  `Item[]`.
- The eleven stance modifier families share one stand-to-crouch-to-prone
  inheritance rule, including the legacy `-10000` unset sentinel and zero
  fallback.

`item_data_staging_model_tests` covers chunked and over-capacity character
callbacks, strict decimal/C-style integer bounds, signed narrowing, stance
inheritance, sparse and unsorted inputs, duplicates, ignored exact-end indices,
zero/default slots, auxiliary retention, required/optional resources,
incomplete and truncated documents, destination-capacity changes,
presence-aware localization, and rollback after a late overlay rejection. A
4-KiB tracked record also proves that base staging constructs exactly one full
candidate and passes it to the publisher by borrowed view, while localized
validation and publication construct no item copy. Both transactions are
non-copyable, and compile-time publisher constraints reject throwing or
failure-returning publication callbacks. The target runs in normal CTest and
the Linux AddressSanitizer matrix. Architecture ratchets keep the model API,
compatibility cases, ownership boundary, documentation, and both test manifests
present.

The headless runtime suite additionally drives the production reader through
the real VFS and Expat adapter. It verifies valid sparse, duplicate, unsorted,
exact-end, and empty base input; empty localized no-op behavior; full rollback
after malformed XML, a missing index, auxiliary overflow, and malformed checked
numeric input inside an out-of-range record; field-aware localized publication
and rollback; valid localized BR/ROF tags ignored for publication; preserved
auxiliary values; and the required-base/optional-localization missing-file
policy. The base publisher derives the checked exclusive high-water value
before its first fixed-array write, and both publishers have exact `void`,
`noexcept` contracts.

Slice 5 will route production character accumulation and UTF conversion through
checked staging, close the remaining `WriteItemStats` writer boundary, remove
the remaining parser-mode surface, and verify escaping plus exact-write failure
cleanup. Until that reader/writer completion slice lands, `XML_Items.cpp`
remains in the remaining-audit inventory below.

## Remaining Utils inventory

The following 12 translation units are compiled and covered by the general
build/test matrix, but have not yet received this same line-by-line ownership,
bounds, and failure-path audit:

- Input and runtime control: `Cursors.cpp`, `Event Pump.cpp`, `KeyMap.cpp`,
  `Timer Control.cpp`, `Utilities.cpp`, and `Win Util.cpp`.
- Data and XML boundaries: `XML_Items.cpp`.
- Image and developer utilities: `Debug Control.cpp`, `MapUtility.cpp`,
  `Quantize Wrap.cpp`, `Quantize.cpp`, and `STIConvert.cpp`.

The next Utils data/XML slice should finish the `XML_Items.cpp` writer boundary.
Input/runtime control and the offline image tools follow.
Existing file formats, resource paths, localization strings, callbacks, visual
layout, and game behavior remain compatibility constraints throughout the audit.
