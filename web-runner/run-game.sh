#!/usr/bin/env bash
# Launched by Xpra as the session's child process (see entrypoint.sh). Runs inside
# the Xpra session, so DISPLAY and PULSE_SERVER are already exported by Xpra.
set -euo pipefail

cd "${GAME_DIR:-/game}"

# Render into Xpra's virtual X display with SDL's software renderer (the game is
# 2D sprite-blitting -- no GPU/GL needed) and route audio to Xpra's PulseAudio sink
# so it reaches the browser.
export SDL_VIDEODRIVER=x11
export SDL_RENDER_DRIVER=software
export SDL_AUDIODRIVER=pulseaudio

echo "[web-runner] launching ${GAME_DIR}/${GAME_BIN:-JA2_ENGLISH}"
exec "./${GAME_BIN:-JA2_ENGLISH}" "$@"
