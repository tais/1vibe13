# Data Package v1

Data Package v1 is an optional startup layer for discovering and selecting
read-only content overlays. It adds package identity, dependencies, validation,
and deterministic load order around the content formats the game already uses.
It does **not** replace or convert those formats.

Existing `Data-*` directories, XML, maps, STI/PNG artwork, sounds, and
`vfs_config.ini` profiles remain valid and unchanged. If no package setting or
package command-line option is present, package discovery is not run and the
legacy startup path is unchanged. An unmanifested installation therefore
continues to work exactly as before.

Data Package v1 is currently data-only and startup-only:

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

The required keys are:

- `MANIFEST_VERSION`: must be `1`.
- `ID`: a unique lowercase identifier containing only `a-z`, `0-9`, `.`, `_`,
  or `-`, with at most 128 characters.
- `VERSION`: a non-empty opaque version of at most 128 characters. Its allowed
  characters are ASCII letters, digits, `.`, `_`, `-`, and `+`.
- `CONTENT_API`: this implementation accepts `1.1` and `1.2`. A package that
  declares `REQUIRES` must use `1.2`; newer or different major versions are
  rejected.
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

Both `PACKAGE_SELECTION` and `REQUIRES` are ordered from lower to higher overlay
priority. Resolution and activation are deterministic: every dependency is
mounted before its consumer, and later IDs in `PACKAGE_SELECTION` can override
earlier read-only content. The resulting priority is conceptually:

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

The v1 host enforces these portability and safety rules:

- at most 32 roots, 4,096 discovered or selected packages, 128 requirements per
  manifest, and 1,000,000 indexed asset files across the startup;
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

Treat Data Package v1 as a strict packaging envelope around trusted legacy mod
content, not as a sandbox for hostile files. It reduces ambiguous discovery and
path behavior, but it does not reinterpret or make the legacy parsers themselves
security boundaries.
