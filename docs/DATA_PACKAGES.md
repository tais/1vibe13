# Data Package v2

Data Package v2 is an optional startup layer for discovering and selecting
read-only content overlays. It adds package identity, dependencies, validation,
and deterministic load order around the content formats the game already uses.
It does **not** replace or convert those formats.

Every v1 manifest remains valid. Version 2 only adds opt-in dependency policy:
optional requirements, declared incompatibilities, and weak ordering edges.

Existing `Data-*` directories, XML, maps, STI/PNG artwork, sounds, and
`vfs_config.ini` profiles remain valid and unchanged. If no package setting or
package command-line option is present, package discovery is not run and the
legacy startup path is unchanged. An unmanifested installation therefore
continues to work exactly as before.

Data Package v2 is currently data-only and startup-only:

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

The repository includes an inert example at
[`examples/data-package`](../examples/data-package). Its asset uses a logical
path the game does not consume, so selecting it demonstrates discovery and
mounting without changing gameplay. From the repository root, its package
arguments are `--package-root examples --package example.readme`.

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

The required keys are:

- `MANIFEST_VERSION`: `1` for the original contract or `2` for dependency
  policy. A v1 manifest cannot use v2 policy keys.
- `ID`: a unique lowercase identifier containing only `a-z`, `0-9`, `.`, `_`,
  or `-`, with at most 128 characters.
- `VERSION`: a non-empty opaque version of at most 128 characters. Its allowed
  characters are ASCII letters, digits, `.`, `_`, `-`, and `+`.
- `CONTENT_API`: this implementation accepts `1.1`, `1.2`, and `1.3`. A package
  that declares `REQUIRES` must use at least `1.2`; a v2 manifest must use
  `1.3`. Newer or different major versions are rejected.
- `TYPE`: `rules`, `extension`, or `tool`. Disk-discovered campaign packages
  are deliberately outside v1; the compiled JA2 or Unfinished Business
  campaign remains the active compatibility bridge.
- `ASSET_ROOT`: a portable, relative, non-empty directory path inside the
  package, such as `Data`.

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
  within the manifest.
- `REQUIRED_CAPABILITIES` uses the same feature-ID format. Every entry must be
  supplied by the host or an active package before bootstrap begins; otherwise
  startup fails before package code runs and exposes the missing ID in
  diagnostics.

An ID may occur only once across `REQUIRES`, `OPTIONAL_REQUIRES`, `CONFLICTS`,
and `LOAD_AFTER`, and no relationship may name its declaring package. Cycles in
strong or optional dependencies and cycles introduced by `LOAD_AFTER` are
reported before activation starts.

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

The v2 host enforces these portability and safety rules:

- at most 32 roots, 4,096 discovered or selected packages, 128 total dependency
  relationships per manifest, and 1,000,000 indexed asset files across startup;
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

Treat Data Package v2 as a strict packaging envelope around trusted legacy mod
content, not as a sandbox for hostile files. It reduces ambiguous discovery and
path behavior, but it does not reinterpret or make the legacy parsers themselves
security boundaries.
