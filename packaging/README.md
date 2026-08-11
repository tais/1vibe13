# Release packaging

Tagged releases retain the four existing engine-only zip archives for Linux
x86_64, Linux ARM64, macOS, and Windows. The same staged payloads also produce
AppImages on both Linux architectures and a per-user NSIS installer on
Windows. No package contains the copyrighted JA2 game data.

## Linux AppImage

The AppImage follows the official
[AppDir layout](https://docs.appimage.org/reference/appdir.html). It bundles
JA2, Unfinished Business, the map editor, and `ja2server`; `AppRun` launches
JA2 by default and accepts `--ja2ub`, `--editor`, or `--server` as its first
argument. Put the AppImage beside `Ja2.ini` and `Data/`, make it executable,
and run it. `--help-engine-package` prints the selector help without game data.
For `--server`, a live `ja2_mp.ini` remains outside the immutable image: it is
read from the current directory, or from beside the AppImage when no `--ini`
argument or current-directory configuration is present. The image contains a
read-only `ja2_mp.ini.sample` for reference.
The AppImage is a deterministic single-file delivery format; it does not claim
an older glibc baseline than the corresponding binary built by the release
runner.

`prepare_appdir.sh` rejects symlinked/missing payloads and normalizes every
AppDir timestamp to the source commit's `SOURCE_DATE_EPOCH`.
`build_appimage.sh` supplies an explicit runtime to `appimagetool`, so the tool
cannot download a moving runtime behind CI's back. The workflow pins both
official AppImage release assets by SHA-256, packages the payload twice, and
requires byte-for-byte equality before publication.

When updating AppImage tooling, obtain both assets from the official
[`appimagetool`](https://github.com/AppImage/appimagetool/releases) and
[`type2-runtime`](https://github.com/AppImage/type2-runtime/releases) release
pages, update the URLs and SHA-256 values together, and retain the two-build
comparison.

Release labels are bounded to a filename-safe 128-character ASCII subset
before they reach version stamping or any output path. A manual workflow run
from a branch whose name contains `/` must provide the optional safe label.

## Windows installer

The native installer is compiled from `windows/ja2-sdl3.nsi` with the official
NSIS 3.12 portable compiler. CI verifies the downloaded NSIS archive against a
committed SHA-256 before extraction. Installation is per-user and does not
request elevation. The installer adds the three game applications, the
standalone server, Start Menu shortcuts, and an Add/Remove Programs entry.

The uninstaller names every owned file explicitly and uses only non-recursive
directory removal. It therefore preserves `Data/`, configuration, mods, saves,
and any other user file. The server configuration is installed as
`ja2_mp.ini.sample`; an existing or customized `ja2_mp.ini` is never overwritten
or removed. The package uses the unique `Uninstall-JA2-SDL3.exe` name instead
of claiming a generic `Uninstall.exe` in an existing game directory. Tagged CI
exercises a silent install/uninstall against a temporary directory and requires
a sentinel under `Data/`, an existing server configuration, and a pre-existing
uninstaller to survive.

## Signing boundary

macOS app bundles continue to receive an ad-hoc signature and strict structural
verification. The AppImages are not GPG-signed and the Windows installer is
not Authenticode-signed because the project has no release signing keys in
GitHub Actions. SHA-256 pinning makes the build inputs immutable, but it is not
a substitute for publisher identity. Production signing requires separately
managed private keys, protected release environments, and a documented key
rotation/revocation procedure; no placeholder secrets are accepted by this
workflow.
