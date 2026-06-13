#!/usr/bin/env bash
# Container entrypoint: start an Xpra session that runs the game and serves its
# display + input + audio to a browser over Xpra's HTML5 client.
set -euo pipefail

: "${GAME_DIR:=/game}"
: "${GAME_BIN:=JA2_ENGLISH}"
: "${XPRA_PORT:=10000}"
: "${DISPLAY_NUM:=:100}"
: "${XPRA_PASSWORD:=}"

# --- preflight: the install must be bind-mounted ------------------------------
if [[ ! -x "${GAME_DIR}/${GAME_BIN}" ]]; then
  echo "FATAL: ${GAME_DIR}/${GAME_BIN} is missing or not executable." >&2
  echo "Bind-mount your Linux JA2 install (the x86_64 ${GAME_BIN} + Data/) at ${GAME_DIR}." >&2
  echo "See web-runner/README.md." >&2
  exit 1
fi

# --- auth on the web/TCP socket ----------------------------------------------
# Without a password, anyone who reaches the port can play AND drive a process on
# this host. Always set XPRA_PASSWORD for anything beyond localhost, and front it
# with a TLS-terminating reverse proxy if it's internet-facing.
BIND="0.0.0.0:${XPRA_PORT}"
if [[ -n "${XPRA_PASSWORD}" ]]; then
  BIND="${BIND},auth=password:value=${XPRA_PASSWORD}"
  echo "[web-runner] web socket auth: password required."
else
  echo "[web-runner] *** WARNING: no XPRA_PASSWORD set -- the port is UNAUTHENTICATED. ***"
fi

# --- clean any stale session from a previous container life -------------------
xpra stop "${DISPLAY_NUM}" >/dev/null 2>&1 || true
rm -f "/tmp/.X${DISPLAY_NUM#:}-lock" "/tmp/.X11-unix/X${DISPLAY_NUM#:}" 2>/dev/null || true

echo "[web-runner] starting Xpra on ${DISPLAY_NUM} -- open  http://<host>:${XPRA_PORT}/"

# --start-child + exit-with-children: the session lives as long as the GAME does
#   (browser disconnects do NOT stop the game -> your campaign keeps running in RAM).
#   If the game exits/crashes, the session ends and Docker's restart policy brings
#   a fresh one up -- pair with JA2 autosave so a crash doesn't lose progress.
exec xpra start "${DISPLAY_NUM}" \
  --start-child="/usr/local/bin/run-game.sh" \
  --exit-with-children=yes \
  --bind-tcp="${BIND}" \
  --html=on \
  --pulseaudio=yes \
  --speaker=on \
  --microphone=off \
  --webcam=no \
  --printing=no \
  --notifications=no \
  --bell=no \
  --mdns=no \
  --daemon=no
