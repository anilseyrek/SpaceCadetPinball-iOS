#!/usr/bin/env bash
# Build the iOS app icon from a single source image.
#   Platform/iOS/tools/make_icon.sh <source-image>
# Produces a 1024x1024 opaque PNG (iOS icons must not have an alpha channel).
set -euo pipefail
SRC="${1:?usage: make_icon.sh <source-image>}"
OUT="$(cd "$(dirname "$0")/.." && pwd)/Assets.xcassets/AppIcon.appiconset/icon_1024.png"
FFMPEG="$(command -v ffmpeg || echo /opt/local/bin/ffmpeg)"
"$FFMPEG" -y -i "$SRC" \
  -vf "scale=1024:1024:force_original_aspect_ratio=increase,crop=1024:1024,format=rgb24" \
  -frames:v 1 "$OUT" 2>/dev/null
echo "wrote $OUT"
