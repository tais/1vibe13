
# 1vibe13, a vibecoded Jagged Alliance 2 v1.13

<br />
<br />

<p align="center">
  <img src="ja2v1.13.png" alt="JA2 v1.13">
</p>

<br />

A **cross-platform** source port of Jagged Alliance 2 v1.13. The original
Win32 / DirectDraw / DirectInput / FMOD / GDI / Smacker stack has been
replaced with portable **SDL3**-based equivalents and a 32-bit colour
rendering pipeline (up from the original 16-bit), so the same code now builds and runs natively on
**Windows, macOS and Linux**. The toolchain is **CMake + clang + Ninja** —
**no Visual Studio / MSVC** project, no `.sln`/`.vcxproj`.

> JA2 v1.13 is a long-running community engine/mod for Jagged Alliance 2.
> Upstream development moved from SVN to GitHub in 2022. This repository is
> the SDL3 cross-platform port of that codebase.

For background and community:
- [The Bear's Pit Forum](https://thepit.ja-galaxy-forum.com)
- [JA2 v1.13 Starter Documentation](https://github.com/1dot13/documentation)
- [The Bear's Pit Discord](https://discord.gg/GqrVZUM)


## ⚠️ Disclaimer

This SDL3 port is **entirely "vibecoded"** — developed rapidly with heavy
AI assistance and a lot of iterative play-testing rather than a formal,
audited process. It is provided **as-is, with absolutely no warranty of any
kind**: no guarantee of correctness, stability, save-game integrity, or
fitness for any purpose, and **no responsibility is accepted for any software
or hardware breakage**, data loss, corrupted saves, or any other mishap that
may result from building or running it. Use at your own risk, keep backups,
and have fun. :)


## What's different in this port

| Area | Was (Win32) | Now |
|---|---|---|
| Platforms | Windows only | Windows, macOS, Linux (one SDL3 codebase) |
| Build | Visual Studio / MSVC | CMake (≥ 3.20) + clang + Ninja, C++17 |
| Window / video | DirectDraw, 16-bit colour | SDL3, 32-bit colour internally |
| Input | DirectInput + Win32 hooks | SDL3 events |
| Audio | FMOD (`fmodvc.lib`) / Miles (`mss32.lib`) | SDL3_mixer |
| Cinematics | Smacker via `binkw32.lib` / `SMACKW32.LIB` | libsmacker (Bink path stubbed — no shipped `.bik`) |
| Dependencies | prebuilt MSVC `.lib` blobs in-tree | built from source (FetchContent) / vendored |
| Display shim | cnc-ddraw | none — SDL3 owns the window & scaling |
| Multiplayer | 32-bit-only third-party transport | Native SDL3_net transport; packaged PvP coordinator; persistent server-authoritative co-op slice with passive JA2 tactical presentation |


## Dependencies

Everything is fetched-from-source or vendored — there are **no system
package requirements** for the libraries themselves and **no prebuilt
`.lib` blobs** checked into the repo.

| Dependency | Version | How |
|---|---|---|
| SDL3 | `main` | FetchContent (`libsdl-org/SDL`) |
| SDL3_mixer | `main` | FetchContent (`libsdl-org/SDL_mixer`) |
| Lua | 5.5.0 | FetchContent (lua.org) |
| Expat | 2.8.1 | FetchContent (`libexpat`) |
| zlib | 1.3.2 | vendored (`ext/zlib`) |
| libpng | 1.6.58 | vendored (`ext/libpng`) |
| utfcpp | 4.1.1 | vendored |
| 7-Zip / LZMA SDK | 26.01 | vendored (used by bfVFS) |
| libsmacker | vendored | `ext/libsmacker` (SMK cinematics) |
| bfVFS | vendored | `ext/VFS` (virtual file system: SLF / 7z / folders) |

SDL3 and SDL3_mixer are tracked from `main` (ahead of any tagged release),
so the bundled runtime libraries must come from this build — see the
release packaging notes below.


## Building from source

Requirements: **CMake ≥ 3.20**, **clang**, **Ninja**, **git**.

```bash
git clone https://github.com/tais/source.git ja2-source
cd ja2-source
cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DLanguages=ENGLISH \
  "-DApplications=JA2;JA2UB;JA2MAPEDITOR"
cmake --build build
```

Build options:

- **`Languages`** (one or more): `CHINESE DUTCH ENGLISH FRENCH GERMAN ITALIAN POLISH RUSSIAN`
- **`Applications`** (one or more): `JA2`, `JA2UB` (Unfinished Business), `JA2MAPEDITOR`, `JA2UBMAPEDITOR`

Each app is built per language, e.g. `JA2_ENGLISH` / `JA2_ENGLISH.exe`.

### Linux

```bash
sudo apt install clang cmake ninja-build pkg-config \
  libwayland-dev wayland-protocols libdecor-0-dev \
  libx11-dev libxext-dev libxcursor-dev libxi-dev libxinerama-dev \
  libxrandr-dev libxss-dev libxkbcommon-dev libxtst-dev libxfixes-dev \
  libgl1-mesa-dev libegl1-mesa-dev \
  libasound2-dev libpulse-dev libdbus-1-dev libudev-dev libibus-1.0-dev
export CC=clang CXX=clang++
# then the cmake commands above
```

### macOS

```bash
brew install cmake ninja        # clang ships with the Command Line Tools
# then the cmake commands above
```

### Windows

Build with **clang targeting the MSVC ABI** (no Visual Studio IDE needed, but
the MSVC SDK/CRT headers must be available — e.g. from the VS Build Tools or
the `ilammy/msvc-dev-cmd` environment in CI):

```bash
export CC=clang CXX=clang++
export CFLAGS=--target=x86_64-pc-windows-msvc
export CXXFLAGS=--target=x86_64-pc-windows-msvc
# then the cmake commands above
```

CI (`.github/workflows/build_unix.yml`) compiles five platform/architecture
targets on every push. On a `v*` tag, `.github/workflows/release.yml` publishes
five explicitly qualified zips and also publishes Linux x64/ARM64 AppImages and
a native Windows installer. See [the packaging contract](packaging/README.md).


## Downloads

Pre-built, per-platform archives are on the
[releases page](https://github.com/tais/1vibe13/releases). Compatibility zip
artifacts are available for Windows x86_64, macOS x86_64 and ARM64 (deployment
target 12.0), and Linux x86_64 and ARM64 (glibc 2.35 ceiling). Every asset name
encodes its architecture; Linux and macOS names also encode their enforced
runtime floor. Linux also has a single-file
AppImage on both architectures, and Windows has a native per-user setup
executable. SDL3, SDL3_mixer, and their codecs are linked statically into each
executable.

Every package contains the **engine only, no game data**. AppImages and the
Windows installer are currently unsigned; download them only from the official
GitHub release.


## Installation

1. Install the original **Jagged Alliance 2** and a working **JA2 1.13** data set.
2. Download the package for your platform. Extract a zip into the JA2 game
   directory, place a Linux AppImage beside `Ja2.ini` and `Data/`, or point the
   Windows installer at that directory.
3. Adjust the `.ini` settings if you like, then run the executable.

No `cnc-ddraw` / DirectDraw shim is required — SDL3 handles the window,
fullscreen and scaling on every platform.


## Project status

The port landed in phases (see [`docs/SDL3_PORT.md`](docs/SDL3_PORT.md) for the
full plan). Window, input, video, audio and cinematics are all SDL3-backed,
colour is 32-bit internally, and the build is CMake-only with dependencies
pulled from source. The `Multiplayer` target owns a project-native framed
SDL3_net transport; the old compatibility shim is gone. The main-menu entry is
enabled, and tagged releases package the data-free `ja2server` coordinator. Its
loopback suite exercises version admission through two-client lobby, placement,
PvP turns/interrupts, late join, and disconnect; that standalone process remains
Deathmatch/Team Deathmatch only because it does not link the game engine.

The full-engine `JA2 --dedicated --dedicated-mode=coop` path now creates or
resumes a campaign-isolated, deterministic strategic save and accepts
transport-bound admission on a configurable endpoint (default
`127.0.0.1:60005`) selected with `--dedicated-coop-bind` and
`--dedicated-coop-port`. A LAN host must explicitly select a reachable interface,
for example `--dedicated-coop-bind=0.0.0.0`. Session epochs, peer identities, and
reconnect bearer tokens come from the operating system's cryptographic random
source. First joins are permissionless and the transport is plaintext, so this
is trusted-LAN/VPN admission, not user authentication or safe public hosting.
The passive client
publishes every accepted bearer before its admission ACK as a private, atomic
224-byte canonical bootstrap + `AdmissionAck` + SHA-256 record outside the
mounted profile. Exact records are idempotent. After installed-content/runtime
compatibility succeeds, a same-epoch record is restored before connecting; an
epoch-only stale record is erased, while corrupt, unsafe, or differently bound
storage fails closed. The live hello must match the preflight epoch before any
admission request. A retained same-epoch durable bearer may retry without an
attempt cap through an intentional server admission blackout; its unsigned
retry counter saturates instead of wrapping. A never-admitted credential-less
client retains the eight-attempt startup limit, and an epoch mismatch fails
closed.
The waiting screen distinguishes opening/admission, automatic reconnection,
campaign transfer, and tactical-baseline readiness. Losing the server reports
reconnection even while the client retains its network composition and seat;
one flushed log records the interruption, and another records recovery only
after campaign readiness is restored. Repeated retries do not repeat the log.

In worldless strategic play, either ready player can order shared travel or
answer supported arrival decisions. Use `Tab` / `]` / `[` to select a squad,
arrow keys to choose an adjacent sector, and `Enter` to submit or `Esc` to cancel.
At a prepared battle, `E` requests the native forced/Spread entry; `R`, then
`Enter`, confirms a permitted retreat. Wilderness notices use `A` to acknowledge
a final destination or `S` to stop a route. The server validates and serializes
these requests, publishes committed observations, and reports the outcome.
Only the designated time leader uses `P` / `1` / `2` / `3` for pause and strategic
speeds. Pending arrival decisions hold time. Shared hiring and spending follow
the same player-permission policy but their remote implementation remains open.

Global co-op protocol v14 also provides explicit voluntary self-retirement.
`L` is deliberately a two-step control: the first key-down must be followed by
a physical release and a later second key-down within the bounded prompt, while
the passive screen continues to drain its complete input FIFO. The exact
24-byte request contains only protocol version, session epoch, and request ID;
it has no peer or victim field, so the ACK-authenticated transport can retire
only its own server-resolved identity. The server preflights a bounded same-
epoch tombstone slot before closing that peer's gameplay authority. Once
accepted, the listener freezes all admission, tactical, and campaign input,
discards queued wire work, and waits only for already-authorized local command
correlations, inbox work, receipts, cancellations, and tracked commands to
drain. It commits the credential tombstone and releases the seat before the
best-effort 48-byte truthful result, then stops and reconciles the network
layers before compacting replication, command, ACL, authority-sequence, Ready,
and participant state. Survivor cursors and receipt histories remain exact; a
distinct fifth identity can take a retired slot in the four-seat roster only
through a fresh baseline and cursor.

The client retains an in-flight leave request and replays its exact ID after a
same-process reconnect ACK instead of silently resuming play. On committed
success it atomically renames the exact private 224-byte bearer to
`client-reconnect-credential.retired` before entering clean `Retired` state or
closing the socket. That marker is idempotent, is validated at startup before
network construction, and remains terminal across server epochs; ambiguous,
corrupt, or unsafe evidence fails closed. There is one explicit
combined-failure boundary: if the server commits retirement, marker publication fails
before the rename (leaving the active bearer), and the server independently
rolls epoch before same-epoch convergence, late verified startup classifies
that active record as stale, erases it, and may fresh-admit. Same-epoch startup
still fails closed, and cross-epoch terminality starts only after the
`.retired` marker publishes.

The production
dedicated server now composes ACK-gated actor assignment, exact-dimension
baseline/delta replication, authoritative execution of fourteen bounded tactical
intent kinds (move, face, stance, stop, end turn, exact-target aimed
single-shot firearm attack, selected-actor reload, and synchronous adjacent
visible-door open/close, exact-serial interrupt pass, adjacent teammate
first-aid initiation, ordinary hand/pocket inventory swap, same-owner item transfer,
exact-tile ordinary ground pickup, and confirmed whole-stack pocket drop) through JA2's command
queue, and ordered receipts.
First aid requires an already equipped usable medical kit and a distinct,
cardinally adjacent wounded teammate. With that medic selected and no targeting
modal active, click the patient's world tile to request aid; roster clicks
remain actor selection. The authority revalidates the exact patient, kit,
skill, service, obstruction, and AP conditions before starting native treatment.
An Applied receipt proves initiation, not completed bandaging. Public medical
state includes bleeding, usable equipped hand-kit points and native providing/
receiving activity. The HUD shows the current published patient and bleeding;
zero bleeding or an Applied receipt alone never marks a service complete.
`Space` requests cancellation; an incapacitated recipient must use the medic.
Press `I` to
inspect the selected owned merc's carried slot summaries and request an ordinary
whole-stack hand/pocket swap: click a source, then destination; `I`/`Esc` closes.
The server checks placement, AP, exact object state and private inventory revision.
With an ordinary source selected, `G` opens a nearby owned merc's inventory;
`Tab`/`[`/`]` cycles recipients, and clicking an empty hand/pocket requests a
whole-stack transfer. `Esc`/`G` cancels without moving anything. The normal
inventory-panel rules require two conscious nearby mercs, line of sight and
3 AP each in combat, charging 2 AP each after placement. Both inventories are
checked again on the server. Cross-player transfers, full item graphs,
attachment/LBE editing, surgery and auto-bandaging
remain outside this slice. See [inventory scope](docs/MULTIPLAYER_ARCHITECTURE.md#owner-inventory-passive-equipping-and-same-owner-transfers).

To drop an ordinary pocket stack, select it in inventory and press `X`, release
the key, then press `X` again to confirm; `Esc` cancels. The server currently
accepts only a clear, dry, empty ground-level tile directly under the merc.
Hands, equipped items, complex stacks and locations requiring relocation are
excluded. One native command moves the entire existing stack and charges the
basic pickup AP cost in combat; no AP is charged in real time.

Inside inventory, `P` browses supported visible ground items on the selected
merc's exact tile and floor. `Tab`/`[`/`]` selects an item; click an empty pocket
to request pickup of the entire stack. `Esc`/`P` cancels. Item names, counts,
condition and ammunition/kit-point totals come from the server; there is no
local item cursor or speculative transfer. The bounded list shows at most 64
items and indicates truncation. Hidden items, owned pools, traps, attachments,
active LBE and other complex objects are not offered; pickup into hands,
stack merging, splitting, automatic walking and drop are not implemented.
Pickup uses the merc's native ground-pickup AP cost, not the transfer cost.

Commands based on an already acknowledged state can now survive consecutive
updates that change only actor animation frames. Any gameplay change still
requires current acknowledged state; movement, AP, turns, equipment, lighting,
palettes, and animation surfaces are not treated as cosmetic. The server keeps
the existing identity, ownership, sequencing, and live gameplay checks, with
no protocol change.
Campaign-ready peers arriving after the first cohort join only at a clean,
fresh-baseline boundary; disconnect does not silently transfer their actors. An
exact untouched initial campaign gets the four cheapest eligible healthy A.I.M.
mercenaries (charge then profile ID), hired on seven-day contracts with profile
equipment and normal finance/history records. That complete roster and its
arrival events are checkpointed
before admission, and the ordinary first-arrival path starts the configured
hostile encounter after clients commit the campaign transfer (immediately for
four ready peers, or after a ten-second gather grace for at least one), and all
four hired actors must become controllable within two minutes.

The same `JA2` executable now has a passive `--coop-client` mode. It obtains the
server's campaign descriptor before random/VFS initialization, installs a
private scratch profile and the exact simulation seed, then mounts data
packages. A distinct rollback-safe `co-op installed content manifest`
subsystem captures and caches installed-content identity after package mounting
and before `legacy content` and `game`; capture failure unwinds the active
package subsystem. Enumeration validates and counts every occurrence before
omitting exclusive runtime directories plus the separately allowlisted exact
root-level `Strategic Decisions.txt` and `DebugMessage.txt` diagnostics, applies the effective normalized
read-only overlay (including case-only spellings across layers), and still
rejects same-layer ambiguity, duplicates, and remaining writable shadows. The
later dedicated/client open uses that cached digest without
re-enumerating content, verifies the runtime fingerprint, transfers and
atomically commits the active campaign checkpoint, then cold-loads it. After
exact outer campaign-identity validation, a restart atomically quarantines any
nonempty disposable client VFS profile to a private
`profile.orphan.<pid>.<seq>` sibling and recreates an empty profile. `Temp`,
`ShadeTables`, settings, and old scratch output move with that tree, while
reconnect credentials and the retired marker remain in the held parent. Both
co-op modes bypass the ordinary `InitMainMenu` transition during INIT, so the
dedicated host reaches its stage-four campaign-entry request and the passive
client reaches its worldless INIT waiting/diagnostic fallback before a pending
main-menu transition can commit. The passive client bypasses `INTRO_SCREEN` and
enters INIT state zero directly; otherwise its INIT-only frame policy would loop
before `InitializeJA2` could open live transport. Only the dedicated
new-campaign branch calls `InitGameOptions()`
immediately before `InitNewGame(FALSE)`: bypassing the interactive settings
screen would otherwise leave an invalid zero difficulty and zero squad capacity.
The canonical defaults select installed difficulty 2 and a six-actor squad;
resume preserves checkpointed options. During a dedicated full load,
`ScopedSavedGameFaceReconstruction` covers the complete synchronous load scope:
saved faces use profile-derived presentation timing without consuming the
canonical RNG, while ordinary face creation retains its three legacy draws.
Strategic AI uses `StrategicAILoadPolicy::DedicatedExactRestore`;
current SAI save v29 restores without compatibility or repair gameplay, and
a stale SAI version is rejected. Tactical snapshot wire v12 carries the
authority's exact
dimensions, the path-safe exact `.dat` map asset basename captured from the
successful world load, canonical actor hostility, a bounded visible-door projection, the
public `commandsBlocked` bit, compact interrupt phase/serial, per-actor
interrupt-action eligibility, ambient light, a bounded 44-byte actor-render
presentation, and five bounded 12-byte combat-equipment records:
primary hand, secondary hand, helmet, vest, and legs. Each record captures only
the first object in that slot: item ID, stack count, and object condition. For
an ammunition-bearing hand object it also captures loaded-ammunition item/count,
signed ammunition condition (including a negative jam state), and whether a
round is chambered; those fields stay canonical zero for ordinary equipment.
During native locomotion, JA2 may advance an actor's logical grid to the next
path tile before its sub-tile render coordinates cross the boundary. Both
values remain independently map-bounded, and the snapshot permits only that
same-or-adjacent-tile transition so a valid walking frame cannot terminate
replication while unrelated coordinates still fail closed.
Production capture failures retain a bounded classification for the server's
observer diagnostic, including invalid actor presentation. These diagnostics
contain no actor identity or payload and clear on the next capture, so a later
successful or unavailable capture cannot reuse an old failure reason.
Older layouts are rejected rather than inferring missing state. The server
keeps native interrupt lists private: `Resolving` blocks input, while `Active`
permits ordinary actions only for exact eligible actors. Pass votes are bound
to both actor incarnation and interrupt serial, and the final eligible player
vote resumes native turn flow; active AI interrupts remain under native AI
control.
Its SDL3_net adapter feeds a transactionally committed snapshot replica. While
campaign, map, baseline, or resynchronization readiness is incomplete, the
worldless `INIT_SCREEN` remains a waiting/diagnostic fallback with an
authority-sized logical-grid diamond, actor-state table, assignment and receipt
status, and typed controls. Once an exact local map plan and its matching
committed baseline are both ready, a transactional presentation host enters the
normal `GAME_SCREEN`. That guarded path replays static JA2 map layers, applies
the authority's ambient light, publishes renderer-only actor proxies, and keeps
selection and the camera on a server-assigned actor. It does not invoke the
legacy tactical interface, panels, radar, scrolling input, visibility
calculation, or any local gameplay phase.
Authority capture always retains player-team actors; a non-player actor enters
the replicated snapshot only while the player team's public opponent knowledge
is exactly current. Losing and reacquiring that knowledge produces ordinary
actor-left/actor-entered deltas.
The same isolated co-op input controller is used by the diagnostic and tactical
views; it drains input before legacy tactical handling can see it. Outside a
modal, the arrow keys submit allocation-free one-tile isometric
moves: Up is row -1/column -1, Down is +1/+1, Left is +1/-1, and Right is
-1/+1. Exact authority dimensions bound both the current and destination tile;
the request never predicts movement into the replica. Tab or `]` selects the
next assigned actor and `[` selects the previous one. `M` retains numeric
destination entry (`R` toggles reverse and Enter submits), `Q`/`E` turn,
`1`/`2`/`3` change stance, Space stops, `T` ends a normal turn or passes the
selected eligible actor's active interrupt, and `R` outside
destination entry reloads the selected actor. `F` enters aimed-fire targeting;
Up/Down/Tab select a replicated opposing actor, `+`/`-` adjust the bounded aim
request, and Enter submits. In the native `GAME_SCREEN`, clicking a TEAM-roster
entry selects that exact assigned actor. A terrain click submits one
server-authoritative move intent; while `F` targeting is active, clicking an
authority-published hostile selects that exact target and Enter confirms the
shot. While `D` mode is active, clicking an exact published same-tile or
cardinally adjacent door selects that door and Enter requests its inverse
state. Clicking the passive radar pans only the local presentation camera;
`Home` recenters on the selected actor and resumes actor following. Radar
navigation preserves any active attack, destination, or door mode and never
emits an intent. Clicks, pins, and the camera outline share the loaded map's
exact absolute radar projection origin, including on rectangular maps. Numeric
movement, attack targeting, and door selection are each bound
to the exact authority world generation and are cancelled on replacement. The
native status strip shows active move, aim, or door details and otherwise
reports the corresponding server receipt state or a distinctly local,
generation-stamped send failure; an exact firearm target also gets a distinct
clipped radar outline. Authority-published doors get persistent closed/open
radar markers below actor pins, with the exact door-mode selection outlined
separately above them.
This input is a bounded, isolated click translator: it does not call
`HandleTacticalUI`, install legacy mouse regions, request local pathing, or run
local tactical simulation. A public `commandsBlocked` value disables every
action and closes any open move, attack, or door modal until a committed
snapshot or same-serial turn delta clears it. Reload carries no client-selected slot
or ammunition: the authority prepares an exact `ReloadWeaponCommand` and
revalidates native `AutoReload`, including manual chambering. The client predicts
neither action-point spend nor damage and does not advance JA2 clocks, AI,
pathing, campaign, or tactical simulation locally. A production-adapter loopback
test drives a real socket through campaign synchronization and reconnect, then
through authoritative Move, aimed-fire, reload, and exact-serial interrupt-pass
deltas with queued/applied receipts.

The generic SdlNet peer retains a 256 KiB/s sustained inbound rate and a 1 MiB
burst. `SdlNetInboundMessageBudget` is configurable only before `Start()` and
remains capped at 32 MiB/s and 4 MiB. Only a full-engine client with a
non-null campaign sink opts into those ceilings; bootstrap, core-only, and legacy peers
retain the strict defaults. A compile-time bound covers one campaign window at
144 FPS. The production socket E2E transfers one exact 11,796,517-byte
checkpoint twice—193 chunks per transfer—at 7 ms pump pacing.

A legal authority flush can publish more named frames than the passive
client's fixed 16-entry callback FIFO, and independent server/client scheduling
means no larger finite FIFO alone can make that safe. The full-engine client
therefore opts into a single-connection SDL parser batch of 16 before startup.
Complete excess frames stay in the transport's bounded input buffer and resume
in exact wire order on later polls; each batch reaches the client core only
after its SDL callbacks have unwound and before a following BYE, malformed
control frame, EOF, or connection-loss notification. A deliberately smaller
configured callback FIFO remains a fail-closed capacity limit.

The authenticated co-op listener keeps tactical and campaign input in separate
bounded shared FIFOs. At saturation, if another transport owns strictly more
queued entries than the incoming sender, that dominant producer is disconnected
and its entries are evicted so the incoming frame can be retained. With no
strictly dominant producer, the incoming sender still fails closed.

`D` enters a modal door selector; Up/Down/Tab cycle only projected doors at the
selected actor's tile or cardinally adjacent tiles, Enter requests the exact
inverse open state, and Esc cancels. The client sends only the public
`{baseGrid, structureId, desiredOpen}` token and never speculates the mutation.
The authority accepts only a visible, adjacent, unlocked, untrapped, ordinary
ground-level door and a controllable non-stealth actor with exact AP/BP and
idle-state preconditions. It revalidates private actor/object fingerprints and
performs one synchronous native partner swap; AP/BP, one door noise, sight,
opponent-list, interrupt, and AI effects occur only after the logical door,
status, graphic, and movement-cost mutation succeeds. A post-swap integrity
failure latches the tactical world and stops replication without charging
points or publishing noise.

The nested wire bounds are explicit: snapshot v12 uses a 314-byte header,
148-byte actor (including five 12-byte combat-equipment records and 44 bytes of
render presentation, plus signed 16-bit bleeding and unsigned 16-bit equipped
hand-kit points and an eight-byte medical-service descriptor), and 7-byte door
record for a 613690-byte generic maximum.
Delta wire v11 permits 10243 generic events, publishes ambient-light changes, and
coalesces all changes to one actor into a single full actor-update event. A
same-turn-serial phase change is one exact 43-byte turn event. The
co-op envelope uses inner tactical wire v6, caps a world at 256 actors, 1024
doors, and 2563 delta events, and bounds the baseline payload/envelope at
45370/46982 bytes and delta payload/envelope at 53853/53925 bytes under the
public 64 KiB ceiling. Tactical intent wire v5 has
an exact 72-byte header, 26-byte maximum payload, and 98-byte maximum record.
First-aid kind 10 carries only a six-byte exact patient identity; command
journal v4 appends `BeginFirstAidCommand` as tag 35 and `SwapInventorySlotsCommand`
as tag 36; `TransferInventoryItemCommand` is tag 37 and
`AuthoritativePickupWorldItemCommand` is tag 38 and
`AuthoritativeDropInventoryItemCommand` is tag 39. Inventory kind 11 carries
two slots and an independent private revision. Transfer kind 12 adds an exact
recipient and that recipient's private revision. Pickup kind 13 adds an exact
ground-item incarnation, empty destination pocket and private revision. Drop
kind 14 carries only the source pocket and private revision (10 payload bytes).
Global session protocol remains
v14; client and server executables are rebuilt together during development.
The service descriptor extends the public actor
suffix. Owner inventory has a separate bounded 6,480-byte
maximum replacement frame, sent only to the current controlling peer. It also
supplies location-only hints for up to 128 discovered ordinary loot piles within
12 tiles of the selected merc, on the same floor. Click a marker to walk to it;
on its tile, click the marker or use `I`, then `P`, to choose a whole-stack pickup
into an empty pocket. A cross marks a pile containing ordinary medical supplies
with remaining points. Hidden and owned piles are not revealed by this view.

Same-connection tactical resynchronization is implemented. When a
client detects a delta-sequence gap, payload-checksum mismatch, state mismatch,
replica rejection, invalid envelope, or rejected baseline, it retains the last
committed view, freezes input, and sends an authenticated, exact 88-byte
self-only resync request. The server answers on the same socket with a fresh
baseline and requires its normal ACK before play resumes. Across rotated
replacement-baseline retries, the server validates the last committed
checkpoint rather than a rejected staged baseline. Admission, socket, identity,
actor assignment, the authoritative command cursor, pending commands, and
receipt history remain intact. A bounded connection-scoped ledger records only
baselines and deltas successfully handed to transport. An exact late ACK may
advance committed recovery evidence without changing the current phase or
staged send; monotonic send ordinals prevent equal-revision regression. An
accepted resync request purges the prior sent-proof set after validating its
committed checkpoint. Exact cursor reconciliation clears an outstanding
command only when an unchanged cursor proves it unconsumed; otherwise its
causal lock survives baseline ACK until retained Queued and terminal receipts
replay within the same tactical world. A newer-world baseline adopts its
authoritative cursor and retires any old-world lock; a late old-world receipt
is then idempotent. A reconnect baseline waits behind a retained pending
command so its causal receipt can drain, while same-connection resync remains
baseline-eligible with that lock intact. Three failed replacement baselines
close the connection. Reconnect remains transport-loss recovery. Focused,
integration, and real-socket tests cover the implemented recovery path.

The six co-op translations that reuse legacy command shapes (end turn, move,
face, stance, stop, and reload) carry
`TacticalCommandAuthorityPolicy::DedicatedCoop`. Only `NetworkPeer` or `Replay`
may carry that policy, and simulation-command journal wire v4 preserves it when
playback substitutes replay provenance. Execution repeats the live world and
controllable on-foot actor checks and, during combat, the player-turn,
no-pending-action, and no-interrupt checks; end turn also rechecks its exact
next team, while reload repeats its weapon/ammunition/chamber/AP validation.
Default `Legacy` commands retain established replica and system behavior. Aimed
fire remains on its separate strict synchronization-source resolver.

The server independently enforces one pending command per peer. An exact-next
pipelined command receives a non-consuming `InvalidCommandSequence` receipt and
may be retried only after the earlier command becomes terminal; it never reaches
gameplay or advances either cursor. Exhausting the global authority sequence is
instead a consuming `AuthoritySequenceExhausted` reason 20. The server stays
active long enough to flush that terminal receipt, and the client records its
exact cursor/history before failing and closing the unrecoverable connection.
Every accepted reservation also captures the coordinator's main-thread pump
frame. A configurable nonzero deadline defaults to 3600 frames. If no terminal
receipt arrives by that boundary, the server pump result identifies the exact
peer and command and fail-stops tactical authority without removing the pending
obligation, advancing its cursor, or fabricating a result for work the engine
may already have applied. The world-drain loop advances the same watchdog, so a
disconnect or unload cannot leave reconnect/admission recovery waiting forever.

Example launch commands (pre-create each state directory as a private directory
owned by that process user; the paths are deliberately absolute and must be
different for server and client):

```bash
/opt/ja2/JA2_ENGLISH --dedicated --dedicated-mode=coop \
  --campaign=lan-demo --campaign-action=new --campaign-seed=20260817 \
  --dedicated-state-dir=/home/ja2/.local/state/ja2/coop-server \
  --dedicated-coop-bind=0.0.0.0 --dedicated-coop-port=60005 \
  --checkpoint-seconds=300

/opt/ja2/JA2_ENGLISH --coop-client --coop-server=192.168.1.50 \
  --coop-port=60005 \
  --coop-state-dir=/home/alice/.local/state/ja2/coop-client
```

An installed-data process certification is registered with CTest when both explicit
CMake cache paths `JA2_COOP_INSTALLED_SMOKE_EXECUTABLE` and
`JA2_COOP_INSTALLED_SMOKE_DATA_ROOT` are configured. It is absent from the
default test graph and currently POSIX-only. The serial harness
selects free loopback ports and private temporary server/client/run roots,
creates a campaign, waits for the client's stable Ready marker, captures its
private 224-byte credential, restarts an independent client on the same state
root, and requires the credential to remain byte-identical at Ready. It then
cleanly stops the still-worldless server through SIGTERM, requires the final
checkpoint and zero exit, then resumes that checkpoint and starts two
independent clients concurrently. A separate bounded tactical deadline covers
both installed-data client cold loads and the starter-actor arrival window. The
harness requires the server's playable-tactical-world marker plus each
client's exact-map-verified, committed-assignment, and tactical-presentation-
ready markers. Dedicated hosts suppress interactive Help overlays
without changing save-owned Help preferences, so first-entry Help cannot block
the server-owned helicopter arrival while waiting for nonexistent local input.
They also take the existing immediate helicopter-skip insertion path instead of
making authoritative actor arrival depend on a presentation-only cinematic.
The harness then restarts client B with its original private state and requires
the same credential, tactical world/map, assigned baseline, and presentation
readiness while client A and the server remain running.
The active-world server then rejects an unsafe cold
checkpoint and exits with its documented failure status. Both clients must
report connection loss and remain alive across repeated automatic retries,
without claiming a restored connection or publishing another Ready marker;
no tactical persistence
is claimed. Installed configuration/content metadata is
fingerprinted before and after, child process groups are deterministically stopped, and temporary state
is removed unless diagnostic retention is explicitly requested; installed data
is never used as writable process state.

The same script accepts `--gameplay-client-executable` to extend that lifecycle
with actual movement. Explicitly rebuild the optional `full_engine_coop_installed_client`
target alongside `JA2_ENGLISH` after code changes and pass its absolute executable
path: that test target is excluded from the default build. This separate test executable
runs the normal installed client, including campaign loading and rendering;
its local stdin driver uses the existing roster/terrain-click controller and
ordinary network intents. It adds no command interface to the shipped game or
server. The harness moves an assigned actor on client A and then on B, waits
for both replicas to reach the exact destination with matching position and
AP, and verifies the moved state again after B restarts. An Applied receipt
alone is insufficient: native movement must finish. The default script run
without this option retains its lifecycle-only coverage.

Add `--verify-transfer` to give and return an existing ordinary ammunition stack
between nearby mercs controlled by one client. This uses the real controller,
requires both private inventory revisions to advance, checks exact whole-stack
summary conservation and both clients' public state, and restores the original
loadouts. It also checks the native 2-AP cost for each actor when the action stays
in one combat phase. This does not certify hidden object graphs or the transfer
panel's mouse-input path. No items are injected and a gameplay rejection fails
the check without replaying it. `--transfer-timeout-seconds` defaults to 60.

Add `--verify-loot` to correlate each client's copied nearby-loot render model
with its current owner-private inventory and both public replicas. This issues
only read-only observations and retains `loot-evidence.json`. Empty or entirely
offscreen marker lists do not certify a nonempty view; this check does not claim
pixel rendering, mouse interaction or pickup. `--loot-timeout-seconds` defaults
to 30 and is capped at 300. It requires the gameplay driver, but not firing.

Add `--verify-drop` to drop one existing ordinary pocket stack and recover that
exact ground-item identity into its original slot. Both replicas must agree on
the actor state; private source/ground summaries and nearby-loot markers must
change coherently, then return to their original contents. Native AP costs must
be positive and equal when both actions remain in the same combat phase. This
uses the controller, not physical X/P key presses, and certifies exposed summary
conservation and the copied marker model, not opaque object graphs or pixels.
No items are injected and a refusal fails without replaying the action.
`--drop-timeout-seconds` defaults to 90 and is capped at 300; the gameplay driver
is required.

Add `--verify-fire` to check one native firing action after the movement and
reconnect checks. The test-only driver uses the existing targeting controller;
both clients must observe ammunition consumption with matching current hand
and AP state, beyond the terminal Applied receipt. It then waits for both
replicas to agree on current shooter/target state and a cleared native command
block before stopping the server. This narrow check does not require a hit or
claim that every projectile effect has finished. Add `--verify-reload` together with `--verify-fire` to
require a subsequent native reload to increase that shooter's primary-hand
ammunition on both replicas. It uses existing reserve ammo; a chamber-only
action or unavailable reserves cannot silently pass as a magazine refill.
Add `--verify-turn-cycle` alongside `--verify-fire` in the gameplay-driver run for one shared-team EndTurn
and explicit serial-bound interrupt passes. It requires both clients to agree
on a stable ordinary player combat turn at least two turn serials later, with
fresh revisions and cleared command gates. The full installed lifecycle passed
this check on 2026-09-05 (turn 4 to 7, one explicit interrupt pass); see the
[multiplayer evidence record](docs/MULTIPLAYER_ARCHITECTURE.md#installed-turn-cycle-evidence).
That run predates the medical wire change and does not certify first aid or
active-battle persistence.

This is a functional, narrow technical co-op slice, not a complete JA2 co-op
experience. The client now has a bounded JA2 static-map and actor viewport plus
adapter-owned native TEAM chrome and a passive exact-map radar with replicated
actor pins. The TEAM chrome now shows an authority-copied controlled roster:
names, AP, simple life/breath bars, static portraits with neutral silhouettes
for unavailable artwork, and bounded
primary/secondary hand condition/jam indicators. Six actors are visible at a
time, and clicking a visible entry selects that exact assigned actor; this is
an ownership view, not a reconstructed strategic squad. Hostile pins are
limited to records admitted by the server's shared team-public
`SEEN_CURRENTLY` visibility gate. Terrain clicks and targeting clicks pass only
through the isolated co-op translator described above; door-mode clicks select
only exact adjacent public door records, and all modal feedback stays in the
same native status strip. Client-side send failures are shown there in red and
cannot masquerade as authoritative receipts or survive into a later tactical
world. These visuals do not
invoke the legacy TEAM/SM panel, radar input, animated faces, full inventory,
clock/town, cursor, mouse regions, or tactical mouse handlers. The
complete interactive tactical interface, per-peer visibility/fog, dynamic world
effects, renderer-facing door proxies, and animated face/gear overlays are not yet present.
The five-slot public combat-equipment projection and owner-only carried-slot
summaries are not full inventory objects: remaining equipment-stack data,
attachments/LBE, cross-player and complex item transfers, general structures, door locks/keys/traps
and asynchronous door work, complete per-peer visibility/opponent knowledge,
bombs, smoke,
projectiles/explosions, interrupt detail, interactions, burst/autofire, melee,
thrown attacks, and many asynchronous actions remain outside the slice. Hiding
opposing markers on the logical plot is presentation only: the actor table still
shows every record that passed the server's public-current-visibility gate,
including non-player actors. That shared-team gate is data minimization, not a
complete per-client fog-of-war or confidentiality system.
Strategic mission/session control is limited, and there is no TLS or public-host
authorization. The opt-in process smoke certifies lifecycle, Ready, same-epoch
credential continuity, final checkpoint/resume, one actual tactical-world start,
and two clients verifying the exact map, committing assigned baselines, and
reaching renderer-ready tactical presentation against one local installed data
set, live tactical client restart, and client recovery status after server loss.
Without the optional gameplay client it submits no gameplay action; with it,
the additional certification covers A/B movement and restored moved state.
This is not a full playthrough, broad interoperability matrix, or soak.
Use client and server executables rebuilt together, with matching installed data,
on a trusted LAN/VPN only. Co-op is still in development; old wire formats and
credentials are not a compatibility commitment. Use fresh private client-state
directories if a stored credential is rejected, preserving the old directories.
The native server checkpoint format is unchanged. In particular, the current mission entry accepts only an
exact untouched initial campaign (including an empty cold resume) or an exact
prepared-initial resume with the complete four-mercenary in-transit roster and
matching delayed-arrival events. A cold established strategic checkpoint with
at least one valid on-foot squad mercenary may resume byte-preserved at entry.
Once a client is campaign-ready, the server enters the canonical hostile sector
occupied by such an actor; vehicle bodies, drivers, passengers, and non-squad
duties never enter the direct-control ACL. A peaceful-only established campaign
stays connected and worldless in strategic idle. At the first committed exact-
victory observation with the gameplay/dialogue/timer hazards clear, the server
immediately stops admission, reconciles Ready peers away, and discards queued
inbound work, so new ACKs or intents cannot starve the return. On the next
committed frame a pure decision rechecks the evidence: regression restores
`Playable` and same-epoch admission, stable evidence waits for the fresh
assignment/receipt/replication boundary, and only then does the server cold-
unload through JA2's normal sector-temp/`TrashWorld` path. It drains the tactical
server, commits a required strategic checkpoint, supersedes campaign transfer,
and only then reopens admission. Native unload owns removal of tactical-only
temporary schedules, and the required cold checkpoint verifies that none survive.
Dedicated co-op suppresses the interactive auto-bandage prompt at victory. This
is one automatic hostile-sector enter/return policy, not a general replicated
strategic map or mission selector. Ambiguous or partial initial states fail
closed rather than being repaired.

Mod authors can optionally wrap existing content in a validated, dependency-
ordered [Data Package](docs/DATA_PACKAGES.md) manifest. Version 4 can select a
complete data campaign in place of the built-in fallback while retaining the
existing `Data-*` directories and file formats. Built-in and external
campaigns both activate over the explicit compiled `ja2.1.13` rules package,
making the campaign → rules → engine layering part of production startup.


## Reports & participation

- Issues / PRs are welcome here on GitHub.
- Bug reports & discussion: [Bear's Pit Forum](http://thepit.ja-galaxy-forum.com/index.php?t=thread&frm_id=216&) · [Bear's Pit Discord](https://discord.gg/GqrVZUM)
