# Browser-playable JA2 via Xpra: runs the native Linux JA2_ENGLISH on a virtual
# X display inside the container and forwards display + input + AUDIO to a browser
# over Xpra's HTML5 client. The game itself is NOT baked in -- you bind-mount your
# JA2 install (binary + Data/) at /game (see docker-compose.yml + README).
#
# Why Xpra (not noVNC): sound is required, and Xpra forwards the app's audio to the
# browser natively (via a per-session PulseAudio sink it manages); plain VNC can't.
# The game runs with SDL's software renderer, so no GPU / GL is needed -- Xpra just
# forwards the X11 window content.
FROM debian:bookworm-slim

ENV DEBIAN_FRONTEND=noninteractive

# Xpra (current, WITH the HTML5 client + audio) comes from the official xpra.org
# apt repo -- Debian's own xpra is 3.1.3 and ships no html5 package. xpra.org
# publishes both amd64 and arm64, so this image builds natively on either arch.
RUN apt-get update && apt-get install -y --no-install-recommends ca-certificates curl gnupg \
 && { curl -fsSL https://xpra.org/gpg.asc; curl -fsSL https://xpra.org/xpra.asc; } \
      | gpg --dearmor -o /usr/share/keyrings/xpra.gpg \
 && echo "deb [signed-by=/usr/share/keyrings/xpra.gpg] https://xpra.org/ bookworm main" \
      > /etc/apt/sources.list.d/xpra.list \
 && apt-get update && apt-get install -y --no-install-recommends \
      # Xpra 6.x is modular: install the server, the X11/seamless component, the
      # html5 web client, SERVER-SIDE audio forwarding, and the CPU picture/video
      # codecs explicitly (the bare `xpra` meta pulls these only via Recommends,
      # which --no-install-recommends skips -> "you must install xpra-x11").
      xpra-server xpra-x11 xpra-html5 xpra-audio-server xpra-codecs xvfb \
      # audio: a session PulseAudio + the gstreamer codecs Xpra encodes speaker with
      pulseaudio pulseaudio-utils \
      gstreamer1.0-plugins-base gstreamer1.0-plugins-good gstreamer1.0-libav gstreamer1.0-tools \
      # runtime libs SDL3 dlopens at launch (it's statically linked into the binary,
      # but still opens the system X11 / audio / dbus client libs at runtime)
      libx11-6 libxext6 libxcursor1 libxi6 libxrandr2 libxinerama1 \
      libxss1 libxkbcommon0 libxfixes3 libxtst6 \
      # keyboard keymap data (xpra warns "XKB bindings not available" without it --
      # the game needs the keyboard) + dbus so the audio session / dbus-launch is clean
      x11-xkb-utils xkb-data dbus dbus-x11 \
      libpulse0 libasound2 libdbus-1-3 \
      # software GL just in case SDL probes it (we force the software renderer anyway)
      libgl1 libegl1 libgl1-mesa-dri \
      fonts-dejavu-core \
 && rm -rf /var/lib/apt/lists/*

COPY entrypoint.sh /usr/local/bin/entrypoint.sh
COPY run-game.sh   /usr/local/bin/run-game.sh
RUN chmod +x /usr/local/bin/entrypoint.sh /usr/local/bin/run-game.sh

# Defaults -- override via docker-compose.yml / `docker run -e ...`.
# (XPRA_PASSWORD is intentionally NOT defaulted here -- it's a secret supplied at
# run time; the entrypoint treats unset as "no auth" and warns.)
ENV GAME_DIR=/game \
    GAME_BIN=JA2_ENGLISH \
    XPRA_PORT=10000 \
    DISPLAY_NUM=:100

EXPOSE 10000
ENTRYPOINT ["/usr/local/bin/entrypoint.sh"]
