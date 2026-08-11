#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 6 ]]; then
	echo "usage: $0 APPDIR OUTPUT ARCH APPIMAGETOOL RUNTIME SOURCE_DATE_EPOCH" >&2
	exit 64
fi

appdir=$1
output=$2
architecture=$3
appimagetool=$4
runtime=$5
source_date_epoch=$6

case "$architecture" in
	x86_64|aarch64) ;;
	*)
		echo "unsupported AppImage architecture: $architecture" >&2
		exit 64
		;;
esac
if [[ ! $source_date_epoch =~ ^[0-9]+$ ]]; then
	echo "SOURCE_DATE_EPOCH must be an unsigned integer" >&2
	exit 64
fi
if [[ ! -d $appdir || -L $appdir ]]; then
	echo "invalid AppDir: $appdir" >&2
	exit 66
fi
for required in AppRun org.ja2v113.JA2SDL3.desktop org.ja2v113.JA2SDL3.png; do
	if [[ ! -e $appdir/$required ]]; then
		echo "AppDir is missing $required" >&2
		exit 66
	fi
done
if [[ ! -x $appimagetool || ! -f $appimagetool || -L $appimagetool ]]; then
	echo "invalid appimagetool: $appimagetool" >&2
	exit 66
fi
if [[ ! -f $runtime || -L $runtime ]]; then
	echo "invalid AppImage runtime: $runtime" >&2
	exit 66
fi
if [[ -e $output || -L $output ]]; then
	echo "refusing to overwrite AppImage output: $output" >&2
	exit 73
fi

# --runtime-file prevents appimagetool from fetching a moving runtime. The
# release workflow checksum-pins both inputs before invoking this script.
ARCH="$architecture" \
SOURCE_DATE_EPOCH="$source_date_epoch" \
APPIMAGE_EXTRACT_AND_RUN=1 \
	"$appimagetool" \
	--runtime-file "$runtime" \
	--comp zstd \
	--no-appstream \
	"$appdir" "$output"

chmod 0755 "$output"
offset=$("$output" --appimage-offset)
if [[ ! $offset =~ ^[0-9]+$ || $offset -le 0 ]]; then
	echo "generated file does not report a valid AppImage payload offset" >&2
	exit 65
fi
