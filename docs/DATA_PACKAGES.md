# Data Packages v1-v4

Data Packages are an optional startup layer for discovering and selecting
read-only content overlays. It adds package identity, dependencies, validation,
and deterministic load order around the content formats the game already uses.
It does **not** replace or convert those formats.

Every older manifest remains valid. Version 2 adds opt-in dependency policy.
Version 3 adds declarative localization documents and opaque, schema-versioned
definition assets through the engine-owned content catalogs. Version 4 makes a
data campaign a selectable peer of the built-in JA2 or Unfinished Business
campaign. The host layers either campaign over the compiled `ja2.1.13@1.13`
rules package, which owns the existing 1.13 table and text bootstrap.

Existing `Data-*` directories, XML, maps, STI/PNG artwork, sounds, and
`vfs_config.ini` profiles remain valid and unchanged. If no package setting or
package command-line option is present, package discovery is not run and the
legacy startup path is unchanged. An unmanifested installation therefore
continues to work exactly as before.

Data Package v4 is currently data-only and startup-only:

- it loads no DLL, shared library, native plugin, or package-supplied code;
- it does not add a new XML, map, artwork, sound, or save-game schema;
- it does not rescan, hot-reload, switch, or unload packages while the game is
  running; and
- its read-only VFS profiles remain mounted until normal VFS shutdown.

## Directory layout

A package root contains one directory per package. Only immediate child
directories are discovered; their folder names do not have to match their
package IDs.

```text
Packages/
  community-balance/
    package.ini
    Data/
      TableData/
        Items.xml
      Maps/
        A9.dat
```

`package.ini` must use that exact filename. `ASSET_ROOT` names a real
subdirectory inside the package. Files below it retain their normal logical VFS
paths, so the example above can override `TableData/Items.xml` and `Maps/A9.dat`
without changing either format.

The repository includes a safe v3 example at
[`examples/data-package`](../examples/data-package). Its asset uses a logical
path the legacy game does not consume, while its localization and definition
files populate the new engine catalogs. Selecting it therefore demonstrates
discovery, mounting, and declared-content import without changing gameplay.
From the repository root, its package arguments are
`--package-root examples --package example.readme`.

[`examples/campaign-package`](../examples/campaign-package) is a safe v4
campaign template. It inherits the installed JA2 data beneath its empty
gameplay overlay, so selecting it exercises campaign replacement without
changing the game:
`--package-root examples --package example.campaign`.

## Manifest

The manifest is an INI file with one `[Package]` section:

```ini
[Package]
MANIFEST_VERSION = 1
ID = community.balance
VERSION = 2.4.0
CONTENT_API = 1.2
TYPE = extension
ASSET_ROOT = Data
REQUIRES = community.rules@2.4.0, community.ui@*
```

A v2 manifest opts into dependency policy with `CONTENT_API = 1.3`:

```ini
[Package]
MANIFEST_VERSION = 2
ID = community.balance
VERSION = 2.5.0-alpha1
CONTENT_API = 1.3
TYPE = extension
ASSET_ROOT = Data
REQUIRES = community.rules@2.4.0
OPTIONAL_REQUIRES = community.ui@*, community.weather@1.1
CONFLICTS = legacy.balance, alternate.overhaul
LOAD_AFTER = community.localization
CAPABILITIES = rules.balance-v2, ui.inventory-overhaul
REQUIRED_CAPABILITIES = engine.rendering, host.networking
```

A v3 manifest opts into declared content with `CONTENT_API = 1.4`:

```ini
[Package]
MANIFEST_VERSION = 3
ID = community.field-kit
VERSION = 1.0.0
CONTENT_API = 1.4
TYPE = extension
ASSET_ROOT = Data
LOCALIZATION = en@Localization/en.lang, nl@Localization/nl.lang
DEFINITIONS = item:community.field-kit@1=Definitions/field-kit.json
```

A v4 campaign opts into startup campaign selection with `CONTENT_API = 1.5`:

```ini
[Package]
MANIFEST_VERSION = 4
ID = community.total-conversion
VERSION = 1.0.0-alpha1
CONTENT_API = 1.5
TYPE = campaign
CAMPAIGN_FAMILY = ja2
ASSET_ROOT = Data
LOCALIZATION = en@Localization/campaign.lang
DEFINITIONS = campaign:community.total-conversion@1=Definitions/campaign.json
```

The required keys are:

- `MANIFEST_VERSION`: `1` for the original contract, `2` for dependency
  policy, `3` for declared content, or `4` for selectable campaigns. Older
  manifests cannot use keys from a newer contract.
- `ID`: a unique lowercase identifier containing only `a-z`, `0-9`, `.`, `_`,
  or `-`, with at most 128 characters.
- `VERSION`: a non-empty opaque version of at most 128 characters. Its allowed
  characters are ASCII letters, digits, `.`, `_`, `-`, and `+`.
- `CONTENT_API`: this implementation accepts `1.1` through `1.5`. A package
  that declares `REQUIRES` must use at least `1.2`; a v2 manifest must use
  at least `1.3`; a v3 manifest must use `1.4`; and a v4 manifest must use
  `1.5`. Newer or different major versions are rejected.
- `TYPE`: `campaign`, `rules`, `extension`, or `tool`. `campaign` requires a
  v4 manifest.
- `ASSET_ROOT`: a portable, relative, non-empty directory path inside the
  package, such as `Data`.

`CAMPAIGN_FAMILY` is required for `TYPE=campaign` and forbidden for other
types. It is `ja2` for the main Arulco executable family or
`unfinished-business` for the UB executable family. The host rejects a
campaign built for the other family before any legacy table, text, grid, or
Lua bootstrap runs. A total conversion targeting the main executable still
uses `ja2`; the family describes the compiled runtime contract, not the
campaign's fictional setting.

Every v4 campaign automatically receives the exact host-managed requirement
`ja2.1.13@1.13`. Do not mention that ID in `REQUIRES`, `OPTIONAL_REQUIRES`,
`CONFLICTS`, or `LOAD_AFTER`; doing so is rejected as an ambiguous attempt to
redefine the application-owned rules boundary.
The host inserts this base layer first; ordinary declared requirements follow
in their stable declaration order and can therefore extend or override it
before the campaign loads. The compiled rules package contributes the portable
`rules.ja2-1.13` capability while active.

`REQUIRES` is optional and is a comma-separated, ordered list. Each entry is
one of:

- `package.id` or `package.id@*` to accept any discovered version; or
- `package.id@exact-version` to require an exact, case-sensitive version
  string.

Version ranges and SemVer comparison are not implemented. Requirements must be
unique, cannot refer to the declaring package, and require `CONTENT_API = 1.2`.
Selecting a package automatically includes its complete requirement closure.

The v2 policy keys are optional comma-separated ordered lists:

- `OPTIONAL_REQUIRES` uses the same `id`, `id@*`, and `id@exact-version` syntax
  as `REQUIRES`. A discovered target joins the dependency closure and receives
  the same version and lifecycle guarantees. An undiscovered target is ignored.
- `CONFLICTS` contains package IDs that must not be active or selected in the
  same closure. Conflict enforcement is symmetric even when only one package
  declares it. An undiscovered target is harmless.
- `LOAD_AFTER` contains weak predecessor IDs. When both packages are already in
  the selected closure, the predecessor loads first. Missing, inactive, or
  unselected targets are ignored; this key never activates another package.
- `CAPABILITIES` contains portable feature IDs contributed while the package is
  active. Engine and mod code can query these IDs at runtime without knowing a
  concrete package ID or build target. Capability IDs use the same lowercase
  portable alphabet and 128-character limit as package IDs, and must be unique
  within the manifest. Provider names beginning with `application.`, `engine.`,
  or `host.` are reserved to the application and are rejected here.
- `REQUIRED_CAPABILITIES` uses the same feature-ID format. Every entry must be
  supplied by the host or an active package before bootstrap begins; otherwise
  startup fails before package code runs and exposes the missing ID in
  diagnostics.

An ID may occur only once across `REQUIRES`, `OPTIONAL_REQUIRES`, `CONFLICTS`,
and `LOAD_AFTER`, and no relationship may name its declaring package. Cycles in
strong or optional dependencies and cycles introduced by `LOAD_AFTER` are
reported before activation starts.

## Declared content

Version 3 adds two optional ordered lists. Paths are portable logical paths
inside `ASSET_ROOT`, are read from the declaring package rather than from a
higher overlay, and participate in normal package bootstrap rollback.

- `LOCALIZATION` entries use `locale@asset/path`. A localization document is
  UTF-8 text beginning with `JA2-LOCALIZATION 1`, followed by `key = value`
  records. Blank lines and lines beginning with `#` or `;` are ignored. Values
  support `\\`, `\n`, `\r`, `\t`, and `\=` escapes. Keys and locale IDs use the
  portable engine identifier alphabet. Later active packages override earlier
  packages for the same locale and key.
- `DEFINITIONS` entries use `type:id@schema=asset/path`. The asset bytes remain
  opaque to Engine/Core; a campaign, mod, or tool resolves the layered record
  and decodes the declared positive integer schema version. This permits JSON,
  XML, or a compact binary domain format without coupling the engine to it.

Imports are bounded to 128 combined declarations per disk package, 4 MiB per
localization document, 65,536 entries per document, 16 KiB per translated
string, and 1 MiB per definition asset. A missing, malformed, oversized, or
catalog-rejected source fails package bootstrap and removes the complete
package-owned content layer.

## Selecting packages

Add comma-separated roots and selected package IDs to the existing `Ja2.ini`:

```ini
[Ja2 Settings]
PACKAGE_ROOTS = Packages, ../SharedPackages
PACKAGE_SELECTION = community.rules, community.balance
```

Relative roots are resolved from the process working directory. When a
selection is configured without any roots, the root defaults to `Packages`.
Configuring roots without a selection discovers, validates, and registers the
packages but activates no external package overlay.

The built-in JA2 or UB campaign is registered as a fallback rather than
pre-activated. An extension/rules/tool-only selection composes over that
fallback automatically. If the selected dependency closure contains a v4 data
campaign, that campaign is selected instead. The package registry still
enforces exactly one active campaign. In both cases activation first selects
the compiled `ja2.1.13` rules package, loads the legacy 1.13 tables and text,
then starts the selected campaign runtime. External package VFS overlays remain
mounted only for data packages; the compiled rules package has no separate
external mount.

The equivalent command-line options are repeatable and also accept comma lists:

```text
JA2_ENGLISH.exe --package-root Packages --package community.rules --package community.balance
```

`--package-root=Packages` and `--package=community.rules,community.balance` are
equivalent forms. If at least one command-line occurrence is present, it
replaces the corresponding INI list rather than appending to it.

`PACKAGE_SELECTION`, `REQUIRES`, and `OPTIONAL_REQUIRES` preserve declaration
order as a stable lower-to-higher overlay-priority input. Strong and present
optional dependencies load before their consumers; `LOAD_AFTER` then adds only
the requested weak edges. Later IDs in `PACKAGE_SELECTION` can override earlier
read-only content when no dependency-policy edge constrains them. The resulting
priority is conceptually:

```text
writable user profile                          highest
later consumer / later selected package
its dependencies / earlier selected package
legacy read-only Data-* and VFS profiles       lowest
```

The writable user-data profile always stays above package overlays. Packages
cannot use this mechanism to shadow saves or other writable user files.

## Validation and failure behavior

Opting in makes validation fail closed: an invalid root, manifest, dependency
graph, asset tree, or VFS mount aborts startup with package/path diagnostics.
All selected mounts are preflighted before package activation begins.
bfVFS cannot remove an arbitrary middle profile, so the host never attempts to
continue after a mount error: earlier profiles may remain present only during
the ensuing fatal shutdown, where normal whole-VFS teardown removes them.

The v4 host enforces these portability and safety rules:

- at most 32 roots, 4,096 discovered or selected packages, 128 total dependency
  relationships per manifest, and 1,000,000 indexed asset files across startup;
  a v4 campaign may author 127 relationships because its host-managed
  `ja2.1.13` requirement occupies the final bounded slot;
- a `package.ini` of at most 64 KiB and at most 250,000 indexed asset files per
  package;
- real package-root/package directories and a regular manifest; symbolic-link
  entries in package roots or the indexed asset tree are rejected;
- an `ASSET_ROOT` that canonicalizes inside its package directory;
- forward-slash relative logical asset paths of at most 1,024 characters and
  components of at most 255 characters, with traversal, absolute paths,
  control characters, Windows device names, and non-portable trailing
  characters rejected; and
- case-insensitive logical lookup compatible with bfVFS. Two physical files
  that normalize to the same logical path, such as `Items.xml` and
  `items.xml`, make the package invalid on every host.

Every discovered package is validated, even if it is not selected. Duplicate
IDs across roots are an error. A package root is scanned deterministically and
may contain ordinary directories without `package.ini`, which are ignored, but
symbolic-link entries are rejected.

Treat Data Package v4 as a strict packaging envelope around trusted legacy mod
content, not as a sandbox for hostile files. It reduces ambiguous discovery and
path behavior, but it does not reinterpret or make the legacy parsers themselves
security boundaries.
