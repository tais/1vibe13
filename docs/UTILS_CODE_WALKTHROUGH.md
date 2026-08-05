# Utils code walkthrough

Status: active architectural refactor, 5 August 2026.

## Scope and completion criteria

`Utils` contains 36 production translation units used by every application
host. This walkthrough tracks them explicitly so a successful local fix is not
mistaken for completion of the directory-wide audit. Two coherent batches now
cover the shared interactive-UI infrastructure and the text/localization
infrastructure. The tactical LBE popup XML loader is included because it is the
persistence boundary for the popup-definition graph.

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

## Remaining Utils inventory

The following 22 translation units are compiled and covered by the general
build/test matrix, but have not yet received this same line-by-line ownership,
bounds, and failure-path audit:

- Input and runtime control: `Cursors.cpp`, `Event Pump.cpp`, `KeyMap.cpp`,
  `Timer Control.cpp`, `Utilities.cpp`, and `Win Util.cpp`.
- Media lifecycle: `Cinematics Bink.cpp`, `Cinematics.cpp`,
  `Music Control.cpp`, and `Sound Control.cpp`.
- Data and XML boundaries: `Encrypted File.cpp`, `INIReader.cpp`,
  `XMLProperties.cpp`, `XMLWriter.cpp`, `XML_Items.cpp`, `XML_Language.cpp`,
  and `XML_SenderNameList.cpp`.
- Image and developer utilities: `Debug Control.cpp`, `MapUtility.cpp`,
  `Quantize Wrap.cpp`, `Quantize.cpp`, and `STIConvert.cpp`.

The next Utils batch should cover the four media lifecycle units because their
audio/video handles and callbacks carry the highest remaining crash and leak
risk. XML/data staging and the offline image tools follow. Existing file
formats, resource paths, localization strings, callbacks, visual layout, and
game behavior remain compatibility constraints throughout the audit.
