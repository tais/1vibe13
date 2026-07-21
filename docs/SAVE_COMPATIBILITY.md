# Save compatibility metadata

New saves retain the established JA2 `.sav` format and receive a neighboring
`.sav.engine-checkpoint` file. The sidecar contains only a bounded,
checksummed engine compatibility fingerprint, active package identities and
versions, and completed engine frame/tick counters. It does not duplicate or
replace campaign state. Older game builds ignore it, and saves created before
this feature remain loadable.

The game removes a sidecar when its save slot is removed or replaced. A new
sidecar is written only after the legacy save closes successfully. Failure to
write this optional metadata is logged but never turns a valid `.sav` into a
failed save.

Load compatibility is checked before the current tactical world, soldiers, or
strategic state is dismantled. The default policy is `warn`: known
incompatibility, corrupt metadata, and storage failures are logged while the
existing save loader remains authoritative. A save with matching metadata and
a legacy save without metadata load normally without a warning.

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

- `ignore`: skip sidecar inspection;
- `warn`: allow loading, but log incompatible, invalid, or unreadable metadata;
- `enforce-known`: reject known-incompatible or invalid metadata while still
  accepting old saves without metadata and allowing transient storage failures
  with a warning; and
- `require-metadata`: reject anything except a matching sidecar, including old
  saves. This is intended for controlled automation rather than ordinary play.

`--no-save-compatibility` overrides an INI setting with `ignore` for one run.
Rejected loads display a short in-game error and retain the current game state;
the log records the path, classification, and active policy.
