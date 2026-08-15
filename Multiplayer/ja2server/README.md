# ja2server — standalone JA2 1.13 multiplayer coordinator

A headless server/relay included with the SDL3 multiplayer port. The
`Multiplayer` target builds the client/server wrapper and SDL3_net compatibility
`netshim`; the main-menu entry is enabled, and tagged releases package
`ja2server`. Those are build, menu, and packaging milestones only: wire
compatibility with in-game clients and end-to-end multiplayer behavior are
experimental and unverified.

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
  server's IP and port; it has not been verified end to end.
- The server and dashboard bind to **127.0.0.1** by default. Set `SERVER_BIND` /
  `DASHBOARD_BIND` to `0.0.0.0` (or a specific interface) for LAN/public play.
- The dashboard status view is read-only and open; **write actions** (save config,
  reset, kick) require `DASHBOARD_TOKEN`. With no token set the controls are
  disabled. When set, open the panel as `http://host:port/?token=SECRET`.
- On Linux/macOS, `kill -HUP <pid>` resets the lobby for a rematch without a
  full restart. (Windows has no SIGHUP; restart the process instead.)
