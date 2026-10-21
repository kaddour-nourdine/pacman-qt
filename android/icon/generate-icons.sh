#!/bin/sh
# Regenerate the legacy launcher bitmaps from the source artwork.
#
# The adaptive icon (API 26+) needs no bitmaps - it is a vector drawable plus
# a colour. These PNGs are only the fallback for older Android versions, so
# they are generated once and committed.
#
# Usage: android/icon/generate-icons.sh
set -e

here=$(cd "$(dirname "$0")" && pwd)
res="$here/../package/res"

if command -v inkscape >/dev/null 2>&1; then
    render() { # <svg> <png> <size>
        inkscape --export-type=png --export-width="$3" --export-height="$3" \
            --export-filename="$2" "$1" >/dev/null
    }
elif command -v rsvg-convert >/dev/null 2>&1; then
    render() { # <svg> <png> <size>
        rsvg-convert -w "$3" -h "$3" -o "$2" "$1"
    }
else
    echo "error: neither inkscape nor rsvg-convert is installed" >&2
    exit 1
fi

# density:size in launcher pixels
for spec in mdpi:48 hdpi:72 xhdpi:96 xxhdpi:144 xxxhdpi:192; do
    density=${spec%%:*}
    size=${spec##*:}
    out="$res/mipmap-$density"
    mkdir -p "$out"
    render "$here/pacman.svg" "$out/ic_launcher.png" "$size"
    render "$here/pacman-round.svg" "$out/ic_launcher_round.png" "$size"
    echo "mipmap-$density: ${size}x${size}"
done
