# Utils code walkthrough

Status: active architectural refactor, 5 August 2026.

## Scope and completion criteria

`Utils` contains 36 production translation units used by every application
host. This walkthrough tracks them explicitly so a successful local fix is not
mistaken for completion of the directory-wide audit. The first coherent batch
deep-audits the shared interactive-UI infrastructure: popup definitions and
instances, popup boxes, merc text boxes, progress bars, sliders, text input,
and word wrapping. The tactical LBE popup XML loader is included because it is
the persistence boundary for the popup-definition graph.

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

## Remaining Utils inventory

The following 28 translation units are compiled and covered by the general
build/test matrix, but have not yet received this same line-by-line ownership,
bounds, and failure-path audit:

- Input and runtime control: `Cursors.cpp`, `Event Pump.cpp`, `KeyMap.cpp`,
  `Timer Control.cpp`, `Utilities.cpp`, and `Win Util.cpp`.
- Text and localization: `Font Control.cpp`, `ImportStrings.cpp`,
  `LocalizedStrings.cpp`, `Multilingual Text Code Generator.cpp`,
  `Text Utils.cpp`, and `message.cpp`.
- Media lifecycle: `Cinematics Bink.cpp`, `Cinematics.cpp`,
  `Music Control.cpp`, and `Sound Control.cpp`.
- Data and XML boundaries: `Encrypted File.cpp`, `INIReader.cpp`,
  `XMLProperties.cpp`, `XMLWriter.cpp`, `XML_Items.cpp`, `XML_Language.cpp`,
  and `XML_SenderNameList.cpp`.
- Image and developer utilities: `Debug Control.cpp`, `MapUtility.cpp`,
  `Quantize Wrap.cpp`, `Quantize.cpp`, and `STIConvert.cpp`.

The next Utils batch should start with `message.cpp`, `Font Control.cpp`, and
`Text Utils.cpp`, because their variadic/fixed-buffer text sinks fan out to
most UI code. Media resource lifecycle is the next-highest crash/leak risk,
followed by XML/data staging and the offline image tools. Existing file
formats, resource paths, localization strings, callbacks, visual layout, and
game behavior remain compatibility constraints throughout the audit.
