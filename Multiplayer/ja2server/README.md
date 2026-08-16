# ja2server — standalone JA2 1.13 multiplayer coordinator

A headless server/relay included with the SDL3 multiplayer port. The
`Multiplayer` target builds the client/server wrapper and native SDL3_net
transport; the main-menu entry is enabled, and tagged releases package
`ja2server`. A data-free loopback test now drives the real coordinator pump
through version admission, a two-client lobby, placement barriers, PvP turns,
interrupts, late join, and disconnect. That exercises the coordinator/session
contract; a complete two-machine playthrough in the game engine remains
experimental.

The standalone executable is a transition product while PvP authority moves
into the full-engine `JA2 --dedicated` host. It supports Deathmatch
(`GAME_TYPE=0`) and Team Deathmatch (`GAME_TYPE=1`) tactical sessions only. It
has no game engine, AI, campaign clock, strategic state, saves, or persistence.
`GAME_TYPE=2` is rejected at startup: co-op requires a full-engine host that
owns those systems.

## Running

```
./ja2server                      # defaults: port 60005, normal logging
./ja2server --verbose            # log sightings, hires, turns, interrupts
./ja2server --dashboard 8080     # web status panel at http://<host>:8080
./ja2server --help               # full flag list
```

Put `ja2_mp.ini` next to the executable to set the server name, port, game type,
log level, etc. Every key is optional; see the comments in the sample file. The
file is **not** required — the server runs on built-in defaults without it.

Flags override the matching `ja2_mp.ini` keys:

| Flag | Overrides |
|------|-----------|
| `--port <n>` | `SERVER_PORT` |
| `--dashboard <n>` | `DASHBOARD_PORT` (0 = off) |
| `--loglevel <n>` | `LOG_LEVEL` (0 normal / 1 verbose / 2 debug) |
| `--verbose` | `LOG_LEVEL = 1` |
| `--debug` | `LOG_LEVEL = 2` |
| `--ini <path>` | which ini file to load |

## Notes

- The experimental client path uses the in-game **Join** screen with the
  server's IP and port. Every peer must use the exact `MP v3.2` protocol build.
- This is an arena-session coordinator, not a co-op campaign or dedicated
  campaign simulation. Lobby progress and scores are in memory only.
- The server and dashboard bind to **127.0.0.1** by default. Set `SERVER_BIND` /
  `DASHBOARD_BIND` to `0.0.0.0` (or a specific interface) for LAN/public play.
- The dashboard status view is read-only and open; **write actions** require
  `DASHBOARD_TOKEN`. Session configuration is accepted only while no players are
  connected and commits transactionally for the next admission. Reset always
  disconnects every transport before reopening the lobby. With no token set the
  controls are disabled. When set, open the panel as `http://host:port/?token=SECRET`.
- Transport and dashboard authentication are intended for trusted networks;
  they do not provide encrypted Internet matchmaking or account identity.
- On Linux/macOS, `kill -HUP <pid>` resets the lobby for a rematch without a
  full restart. (Windows has no SIGHUP; restart the process instead.)
