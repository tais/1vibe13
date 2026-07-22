# Save compatibility metadata

New saves retain the established JA2 `.sav` format and may receive two
neighboring files. `.sav.engine-checkpoint` contains a bounded, checksummed
engine compatibility fingerprint, active package identities and versions, and
completed engine frame/tick counters. When an active package declares a save
schema, `.sav.engine-packages` stores its bounded opaque campaign-state record
in deterministic activation order. Archive v2 also checkpoints the engine-owned
deterministic random streams for every active package, including packages with
no opaque campaign payload. Neither file duplicates or replaces JA2's tactical
or strategic state. Older game builds ignore both, and saves created before
this feature remain loadable; the loader still accepts the exact v1 archive.
For v1 sidecars, compatibility is validated against the reconstructed pre-v2
runtime fingerprint (the same configuration projection the older host hashed),
not skipped or weakened; v2 sidecars must match the complete current fingerprint.

The game removes both sidecars when their save slot is removed or replaced.
New sidecars are written only after the legacy save closes successfully. A
runtime with no active package state removes any obsolete package archive.
Failure to capture or write optional engine/package metadata is logged but
never turns a valid `.sav` into a failed save.

Load compatibility is checked before the current tactical world, soldiers, or
strategic state is dismantled. The default policy is `warn`: known
incompatibility, corrupt metadata, missing package state, and storage failures
are logged while the existing save loader remains authoritative. A matching
archive is structurally checked before teardown and package callbacks restore
only after every legacy load step succeeds. A sidecar-free legacy save still
loads; it emits a warning only when the active runtime contains a package that
would otherwise have restored campaign state.

Random restore is one engine transaction across the active package set. Every
replacement map is prepared before callbacks or live mutation, callback draws
are discarded, and the prepared maps commit with no-throw swaps only after all
package validation and load callbacks succeed. Legacy v1 archives do not rewind
random streams, and failed restores leave every package's live streams intact.

Configure the policy in `Ja2.ini`:

```ini
[Ja2 Settings]
SAVE_COMPATIBILITY_POLICY = warn
```

The equivalent command-line form is:

```text
JA2_ENGLISH.exe --save-compatibility=enforce-known
```

Accepted policies are:

- `ignore`: skip sidecar inspection and package-state restoration;
- `warn`: allow loading, but log incompatible, invalid, or unreadable metadata;
- `enforce-known`: reject known-incompatible or invalid metadata while still
  accepting old saves without metadata and allowing transient storage failures
  with a warning; and
- `require-metadata`: reject anything except a matching sidecar, including old
  saves. This is intended for controlled automation rather than ordinary play.

`--no-save-compatibility` overrides an INI setting with `ignore` for one run.
Rejected preflights display a short in-game error and retain the current game
state; the log records the path, classification, and active policy. A package
restore callback failure after a successful legacy load is recorded as a
runtime fault and log error, but cannot safely roll back the already committed
legacy game state.
