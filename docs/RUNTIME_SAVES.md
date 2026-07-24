# Runtime save container

The current game save is one `.sav` file owned at its outer boundary by the
engine. It contains:

1. the application/domain byte stream at offset zero;
2. a bounded engine section table; and
3. a fixed 32-byte trailer describing and checksumming both regions.

Keeping the domain stream at offset zero is an incremental migration seam. The
existing save-slot preview and domain loader can still read their established
fields while individual domain serializers move behind engine interfaces. It
is not a backward-compatibility promise: a complete engine container is
mandatory.

## Required sections

Every save contains exactly one of each current required section:

- `CHKP`: the runtime fingerprint, active package identities and versions, and
  the completed frame/tick boundary; and
- `PGST`: package-defined per-save payloads plus the engine-owned deterministic
  random checkpoint for every active package.

Both payloads use bounded, versioned, checksummed persistence envelopes. The
package archive is version 3; it always carries an explicit engine-record count
and has no legacy “engine state absent” mode.

Unknown unique section types are accepted as a forward-extension point.
Missing required sections, duplicate section types, type zero, malformed
lengths, and trailing bytes are rejected.

## Trailer

All integers are little-endian. The trailer contains:

| Field | Size |
|---|---:|
| magic `J2SC` | 4 bytes |
| container version (`1`) | 2 bytes |
| flags (`0`) | 2 bytes |
| exact domain length | 8 bytes |
| exact section-table length | 8 bytes |
| domain FNV-1a checksum | 4 bytes |
| section-table FNV-1a checksum | 4 bytes |

The checksums detect accidental corruption; they are not an authentication
mechanism. Exact lengths prevent truncation, appended garbage, or one region
being interpreted as another.

## Save transaction

At the paused game boundary the application captures one immutable runtime
checkpoint and package snapshot. It then writes and closes the domain stream.
Only after a successful close does `RuntimeSaveContainerService` append the two
encoded sections and trailer in one storage write.

A capture, encode, or seal failure makes the whole save fail and removes the
incomplete `.sav`. Runtime metadata is never optional and no neighboring files
are created.

## Load transaction

Before the current tactical or strategic world is dismantled, load preflight:

1. reads the complete file through configured bounds;
2. validates trailer version, flags, exact lengths, and both checksums;
3. validates the unique section table and both required sections;
4. requires the exact current runtime fingerprint;
5. decodes the package archive; and
6. validates every active package identity, version, schema, and engine record
   without mutating live state.

Only then may the application domain loader run. The already staged package
snapshot is restored once after the domain load succeeds, so later file changes
cannot change what was preflighted. Before closing the domain reader, the
application also requires its exact final offset to equal the trailer's domain
length; it cannot silently leave bytes unread or consume engine sections.

## Compatibility policy

There is deliberately no compatibility policy or command-line bypass. Plain
domain-only saves, the former neighboring metadata files, older package
archives, mismatched package graphs, and corrupt containers are rejected before
destructive load. This project chose a clean development-format break so the
runtime has one enforceable invariant instead of optional metadata paths.

The default independent limits are 64 MiB for the domain region, 64 MiB for the
section table, and 64 sections. Hosts publish them as:

- `engine.runtime-save.domain-byte-limit`
- `engine.runtime-save.container-byte-limit`
- `engine.runtime-save.section-limit`

The engine-core tests cover prefix preservation, exact section round trips,
transactional failure output, integrity rejection, invalid section contracts,
and independent bounds. Headless application tests cover capture/seal/preflight/
restore, fingerprint and package-contract rejection, backing-file changes after
preflight, and incomplete-save cleanup.
