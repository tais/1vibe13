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

### Native reinforcement extension

New native game saves also contain `RINF`, an optional application section
whose version-1 payload is exactly 24 bytes: six little-endian `uint32` values
for version (`1`), tactical turn counter, enemy reinforcement deadline, enemy
arrival backlog, militia reinforcement deadline, and militia arrival backlog.
It is captured before the domain writer runs and validated during preflight.
After sector reconstruction, native loading restores all five gameplay values
without drawing random numbers. A present section with an unsupported version
or incorrect length rejects the load before domain mutation.

Existing containers without `RINF` retain their historical reinforcement
deadline estimates and conditional RNG draws. The domain prefix and save-game
version are unchanged. Strict dedicated saves accept this known extension in
addition to their required `CHKP`, `PGST`, and random-checkpoint sections; other
unknown sections remain unsupported by that policy. This state preservation
does not enable active tactical dedicated checkpoints: their eligibility and
host/client resume restrictions remain in force.

### Native schedule extension

New native game saves contain optional application section `SCHD`. Version 1
stores the complete ordered schedule list, including temporary defaults and
unbound authored schedules, plus the exact allocation counter. All integers
are little-endian. Its 8-byte header contains a `uint32` version (`1`), a
`uint16` node count (at most 255), a `uint8` allocation counter, and a zero
reserved byte. Each 53-byte node contains the schedule ID (`uint8`), flags
(`uint16`), actor slot (`uint16`, `0xffff` means unbound), actor incarnation
(`uint32`, zero only for unbound nodes), four times (`uint16` each), two arrays
of four action parameters (`uint32` each), and four actions (`uint8` each).

Preflight rejects unsupported versions, incorrect lengths, invalid fields,
duplicate IDs, and duplicate bound actor slots before domain mutation. After
native actors and strategic events load, restoration validates each bound
actor's saved identity and schedule ID, every active actor's schedule binding,
and each queued tactical schedule event's referenced ID. Out-of-sector actors
remain valid bindings. The replacement list is fully allocated before the old
list is changed, and the loaded event queue is preserved.

A present section replaces final `PostSchedules` reconstruction, consuming no
random numbers and posting no duplicate events. A missing section retains the
exact historical reconstruction and its RNG draws. The legacy domain stream
is unchanged. This is a state preservation prerequisite; active tactical
dedicated checkpoint eligibility remains unchanged.

The native schedule test uses a real disk container with an opaque domain
prefix and production schedule, event, and RNG state. It checks restoration,
malformed payload rejection, and the legacy reconstruction negative control;
it does not qualify a full installed `SaveGame`/`LoadSavedGame` round trip.
### Passive campaign display projection

New dedicated saves contain `PCVW`, an owned passive-client projection. Version 1
uses little-endian integers: a 20-byte header contains version (u32), world kind
(u8: strategic 1 or tactical 2), sector X/Y/Z (u8 each), map rows/columns (u16
each), world minutes (u32), profile count (u16), and reserved zero (u16).
Each 130-byte profile record contains its ID (u16) and 32 Unicode scalar values
(u32 each), terminated and padded with zeros. At most 255 strictly ordered,
unique IDs below 255 are accepted. Nonempty names must fit a 32-unit UTF-16
display buffer including the terminator; malformed scalars and control values
below 32 or equal to 127 are rejected. Strategic descriptors have zero sector
and dimensions; tactical descriptors use X/Y 1–16, Z 0–3 and dimensions 1–2000.
The maximum payload is 33,170 bytes.

Passive clients require this section, exact transferred world minutes, and the
existing strict runtime/package compatibility preflight. They publish the
owned projection only after the preflight transaction rolls back cleanly.
They never load the authority domain, restore its RNG/frame state, or create
and then unload a native tactical world. AIM labels use the owned catalog;
live roster, portrait, rendering, campaign and inventory data remain supplied
by their existing authoritative snapshots.

Native host loading still accepts older containers without `PCVW`. Before
admitting peers, an already supported cold strategic resume with a missing
projection goes through the existing eligibility checks and atomic checkpoint
publication to produce a new generation. A failed refresh retains the previous
durable generation. Present malformed projections reject during preflight.
Runtime fingerprints are unchanged. Same-protocol older clients reject PCVW
at the strict unknown-section check before authority loading; newer clients
reject checkpoints without PCVW. They may connect before this late compatibility
rejection. A session protocol bump in another feature does not distinguish two
builds sharing that protocol. This descriptor does not enable tactical
checkpoint eligibility or active battle resume.

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
