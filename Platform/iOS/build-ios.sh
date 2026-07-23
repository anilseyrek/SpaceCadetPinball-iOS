#!/usr/bin/env bash
#
# Configure and build SpaceCadetPinball for iOS.
#
# Requires the FULL Xcode (not just Command Line Tools):
#   sudo xcode-select -s /Applications/Xcode.app/Contents/Developer
#
# Usage:
#   Platform/iOS/build-ios.sh [simulator|device] [Debug|Release]
#
# Default target is the iOS Simulator (no code signing / Apple account needed).
# Building for a physical device requires a development team; set
# SCP_DEV_TEAM=XXXXXXXXXX (your 10-char Team ID) in the environment.

set -euo pipefail

PLATFORM="${1:-simulator}"     # simulator | device
CONFIG="${2:-Debug}"           # Debug | Release
REPO_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
BUILD_DIR="$REPO_ROOT/build-ios-$PLATFORM"

if ! xcode-select -p 2>/dev/null | grep -q "Xcode.app"; then
    echo "ERROR: full Xcode is not selected."
    echo "Run: sudo xcode-select -s /Applications/Xcode.app/Contents/Developer"
    exit 1
fi

case "$PLATFORM" in
    simulator) SYSROOT=iphonesimulator ;;
    device)    SYSROOT=iphoneos ;;
    *) echo "Unknown platform '$PLATFORM' (use simulator|device)"; exit 1 ;;
esac

echo ">> Configuring ($PLATFORM, $CONFIG) into $BUILD_DIR"
cmake -S "$REPO_ROOT" -B "$BUILD_DIR" -G Xcode \
    -DCMAKE_SYSTEM_NAME=iOS \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=13.0 \
    -DCMAKE_OSX_SYSROOT="$SYSROOT" \
    -DCMAKE_XCODE_ATTRIBUTE_ONLY_ACTIVE_ARCH=NO \
    ${SCP_DEV_TEAM:+-DCMAKE_XCODE_ATTRIBUTE_DEVELOPMENT_TEAM="$SCP_DEV_TEAM"}

echo ">> Building"
cmake --build "$BUILD_DIR" --config "$CONFIG" -- -sdk "$SYSROOT"

echo ""
echo ">> Done. App bundle:"
find "$BUILD_DIR" -name "SpaceCadetPinball.app" -maxdepth 4 -print
echo ""
echo "To run in the Simulator:"
echo "  xcrun simctl boot 'iPhone 15' 2>/dev/null || true"
echo "  open -a Simulator"
echo "  xcrun simctl install booted <path-to-SpaceCadetPinball.app>"
echo "  xcrun simctl launch booted com.example.spacecadetpinball"
