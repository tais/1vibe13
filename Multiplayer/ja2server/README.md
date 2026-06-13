# ja2server — standalone JA2 1.13 multiplayer coordinator

A headless server/relay for SDL3-port multiplayer. It speaks the same wire
protocol as the in-game host, so stock clients connect to it unchanged — no one
has to keep a full game instance running just to host. It owns the lobby, turn
order, the interrupt arbiter, the win check and the scoreboard.

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

- Clients connect with the in-game **Join** screen using the server's IP and port.
- The web dashboard is **unauthenticated** — only enable it on a trusted LAN.
- On Linux/macOS, `kill -HUP <pid>` resets the lobby for a rematch without a
  full restart. (Windows has no SIGHUP; restart the process instead.)
