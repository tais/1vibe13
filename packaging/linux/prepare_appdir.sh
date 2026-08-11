#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 3 ]]; then
	echo "usage: $0 PAYLOAD_DIR APPDIR SOURCE_DATE_EPOCH" >&2
	exit 64
fi

payload_dir=$1
appdir=$2
source_date_epoch=$3
script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)
repository_root=$(cd -- "$script_dir/../.." && pwd -P)

if [[ ! $source_date_epoch =~ ^[0-9]+$ ]]; then
	echo "SOURCE_DATE_EPOCH must be an unsigned integer" >&2
	exit 64
fi
if [[ ! -d $payload_dir || -L $payload_dir ]]; then
	echo "payload is not a real directory: $payload_dir" >&2
	exit 66
fi
if [[ ! -d $payload_dir/ja2server || -L $payload_dir/ja2server ]]; then
	echo "server payload is not a real directory: $payload_dir/ja2server" >&2
	exit 66
fi
if [[ -e $appdir || -L $appdir ]]; then
	echo "refusing to replace existing AppDir: $appdir" >&2
	exit 73
fi

required_payload=(
	JA2_ENGLISH
	JA2UB_ENGLISH
	JA2MAPEDITOR_ENGLISH
	ja2server/ja2server
	ja2server/ja2_mp.ini
	ja2server/README.md
)
for relative_path in "${required_payload[@]}"; do
	path="$payload_dir/$relative_path"
	if [[ ! -f $path || -L $path ]]; then
		echo "missing or unsafe AppImage payload file: $path" >&2
		exit 66
	fi
done

install -d \
	"$appdir/usr/bin" \
	"$appdir/usr/lib/ja2server" \
	"$appdir/usr/share/applications" \
	"$appdir/usr/share/doc/ja2-sdl3"

for application in JA2_ENGLISH JA2UB_ENGLISH JA2MAPEDITOR_ENGLISH; do
	install -m 0755 "$payload_dir/$application" "$appdir/usr/bin/$application"
done
install -m 0755 "$payload_dir/ja2server/ja2server" \
	"$appdir/usr/lib/ja2server/ja2server"
install -m 0644 "$payload_dir/ja2server/ja2_mp.ini" \
	"$appdir/usr/lib/ja2server/ja2_mp.ini.sample"
install -m 0644 "$payload_dir/ja2server/README.md" \
	"$appdir/usr/lib/ja2server/README.md"

install -m 0755 "$script_dir/AppRun" "$appdir/AppRun"
install -m 0644 "$script_dir/org.ja2v113.JA2SDL3.desktop" \
	"$appdir/org.ja2v113.JA2SDL3.desktop"
install -m 0644 "$script_dir/org.ja2v113.JA2SDL3.desktop" \
	"$appdir/usr/share/applications/org.ja2v113.JA2SDL3.desktop"
install -m 0644 "$repository_root/ja2v1.13.png" \
	"$appdir/org.ja2v113.JA2SDL3.png"
install -m 0644 "$repository_root/packaging/README.md" \
	"$appdir/usr/share/doc/ja2-sdl3/README.md"
ln -s "org.ja2v113.JA2SDL3.png" "$appdir/.DirIcon"

# appimagetool passes these mtimes to SquashFS. Normalize every entry to the
# source commit time so repeated packaging of the same payload is byte-stable.
while IFS= read -r -d '' entry; do
	touch --no-dereference --date="@$source_date_epoch" "$entry"
done < <(LC_ALL=C find "$appdir" -depth -print0 | LC_ALL=C sort -z)
