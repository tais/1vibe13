# web-runner — play JA2 in a browser, streamed from the server

Runs the **native Linux `JA2_ENGLISH`** on a headless virtual X display inside a
container and streams its **display + input + audio** to a web browser via
[Xpra](https://xpra.org)'s HTML5 client. No game code is changed and no WebAssembly
is involved — it's the real binary running natively, just with its screen piped to
a browser tab.

The session is **persistent**: the game lives in the container's RAM, so you can
close the tab and reconnect days later to the same in-progress campaign — no
save/load cycle in normal use.

```
browser ──HTTP/WebSocket──► Xpra (HTML5 + audio) ──► JA2_ENGLISH on virtual X :100
                                                       └─ your install bind-mounted at /game
```

## Prerequisites
- A Linux **x86_64** host with Docker + Docker Compose. (The container is x86_64;
  the bind-mounted `JA2_ENGLISH` must be the **Linux x86_64** build — e.g. the one
  from the `ja2-sdl3-linux-*.zip` release, dropped into a full JA2 1.13 install.)
- A JA2 install directory on the host containing `JA2_ENGLISH`, `Data/`, `ja2.ini`,
  and the usual `vfs_config*.ini` — i.e. a normal playable install.

## Quick start
```bash
cd web-runner
GAME_DIR=/path/to/your/JA2-install \
XPRA_PASSWORD=pick-a-password \
docker compose up -d --build
```
Open **`http://<host>:10000/`**, enter the password, and you're in the game. Click
the page once if the browser hasn't enabled audio yet (browsers gate autoplay
behind a user gesture).

Stop / restart / logs:
```bash
docker compose logs -f          # watch the game + Xpra output
docker compose restart          # restart the session (loses the in-RAM game)
docker compose down             # stop
```

## Recommended `ja2.ini` settings
- **Windowed mode** (not fullscreen) streams most cleanly through Xpra. Set the
  screen mode to a non-fullscreen value (`iScreenMode` != 0) so the engine opens a
  normal resizable window. (It still works fullscreen, but windowed is tidier.)
- Pick a resolution you want in the browser (e.g. 1024×768). The 640×480 UI is
  letterbox-scaled to fit.

## Persistence & the one caveat
The whole point: the process stays resident, so the campaign sits in RAM between
visits. **But** a container crash or redeploy *does* lose that RAM state, and the
restart policy will start a fresh game. So treat JA2's **autosave** (it saves on
sector transitions) as the recovery floor — saves land in your bind-mounted install
on the host and survive container restarts. The "never manually save" experience
holds for a stable session; autosave is just the safety net.

## Security ⚠️
Whoever reaches port 10000 can **play your game and drive a process on your host**.
- **Always set `XPRA_PASSWORD`** (the entrypoint warns loudly if it's empty).
- For anything internet-facing, put a **TLS-terminating reverse proxy** (Caddy /
  nginx) in front so the WebSocket is `wss://` and add your own auth layer. Don't
  expose the raw port to the internet.

## Audio (it's essential, so here's how to verify)
Audio flows: game → SDL (`SDL_AUDIODRIVER=pulseaudio`) → Xpra's per-session
PulseAudio sink → Xpra encodes it (gstreamer) → browser plays it. The container
ships `gstreamer1.0-plugins-{base,good}` + `libav` to cover Xpra's speaker codecs.
- In the HTML5 client, make sure the **speaker** toggle is on and the tab isn't
  muted; click the page once to satisfy browser autoplay.
- If you get video but no sound, check `docker compose logs` for Xpra speaker/
  gstreamer errors and try a different codec (`--speaker-codec=opus` or `vorbis`
  in `entrypoint.sh`).

## How it works (files)
- `Dockerfile` — Debian + Xpra + Xpra-HTML5 + PulseAudio + the X11/audio runtime
  libs SDL3 dlopens. The game is **not** baked in; it's bind-mounted.
- `entrypoint.sh` — starts the Xpra session, wires up auth + audio, runs the game.
- `run-game.sh` — the session child: `cd /game`, force software renderer +
  pulseaudio, exec the binary.
- `docker-compose.yml` — port, the `GAME_DIR` bind-mount, password, restart policy.

## Troubleshooting first run
- **`JA2_ENGLISH is missing or not executable`** — `GAME_DIR` doesn't point at the
  install, or the binary is the wrong arch (must be Linux x86_64).
- **Black screen** — confirm the game launched (`docker compose logs`); a software
  renderer + missing `Data/` is the usual cause.
- **Window too big/small** — set the resolution in `ja2.ini`; the client scales.
- **One player at a time** — one container = one running game. For multiple players,
  run multiple containers on different ports.
