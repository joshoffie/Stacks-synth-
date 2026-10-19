#!/bin/sh
# Builds the signed macOS installer: Stacks-<version>.pkg with the Audio Unit,
# the VST3, the standalone app and the sound engine data.
#
#   Packaging/build-installer.sh [--model /path/to/engine.gguf] [--notary-profile NAME] [--skip-build] [--update-only]
#
# Produces dist/Stacks-<version>.pkg (full: plug-ins, app and the engine data,
# for a first install) and dist/Stacks-<version>-update.pkg (plug-ins and app
# only, a few MB, for everyone who already has the engine data). --update-only
# skips the full package and does not need the engine file.
#
# Signing uses the Developer ID certificates in the login keychain. Pass
# --notary-profile with a profile made by `xcrun notarytool store-credentials`
# to notarise and staple the package so it opens on another Mac with no
# Gatekeeper detour. Without it the package is signed but not notarised.
set -eu

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="$ROOT/build.nosync"
OUT="$ROOT/dist"
# Staged outside the project: iCloud Drive re-adds Finder attributes that codesign rejects.
WORK="$(mktemp -d "${TMPDIR:-/tmp}/stacks-installer.XXXXXX")"
STAGE="$WORK/stage"
trap 'rm -rf "$WORK"' EXIT
APP_ID="Developer ID Application"
PKG_ID="Developer ID Installer"
MODEL=""
PROFILE=""
SKIP_BUILD=0
UPDATE_ONLY=0

while [ $# -gt 0 ]; do
    case "$1" in
        --model) MODEL="$2"; shift 2 ;;
        --notary-profile) PROFILE="$2"; shift 2 ;;
        --skip-build) SKIP_BUILD=1; shift ;;
        --update-only) UPDATE_ONLY=1; shift ;;
        *) echo "unknown option $1" >&2; exit 2 ;;
    esac
done

VERSION="$(sed -n 's/^project(Stacks VERSION \([0-9.]*\).*/\1/p' "$ROOT/CMakeLists.txt")"
[ -n "$VERSION" ] || { echo "could not read the version from CMakeLists.txt" >&2; exit 1; }

# The engine data: the user's own copy, else the shared one, else a path given.
if [ -z "$MODEL" ]; then
    for candidate in "$HOME/Library/Application Support/Stacks/models/Qwen3-4B-Q4_K_M.gguf" \
                     "/Library/Application Support/Stacks/models/Qwen3-4B-Q4_K_M.gguf"; do
        [ -f "$candidate" ] && MODEL="$candidate" && break
    done
fi
if [ "$UPDATE_ONLY" = 0 ]; then
    [ -f "$MODEL" ] || { echo "engine data not found; pass --model /path/to/Qwen3-4B-Q4_K_M.gguf (or --update-only)" >&2; exit 1; }
fi

echo "== Stacks $VERSION"
[ "$UPDATE_ONLY" = 0 ] && echo "   engine data: $MODEL ($(du -h "$MODEL" | cut -f1))"

if [ "$SKIP_BUILD" = 0 ]; then
    echo "== building"
    cmake -S "$ROOT" -B "$BUILD" -DCMAKE_BUILD_TYPE=Release >/dev/null
    cmake --build "$BUILD" --target Stacks_Standalone Stacks_AU Stacks_VST3
fi

ART="$BUILD/Stacks_artefacts/Release"
rm -rf "$STAGE"
mkdir -p "$STAGE/app" "$STAGE/au" "$STAGE/vst3" "$STAGE/engine/Library/Application Support/Stacks/models" "$WORK/pkgs" "$STAGE/res" "$OUT"
cp -R "$ART/Standalone/Stacks.app" "$STAGE/app/"
cp -R "$ART/AU/Stacks.component" "$STAGE/au/"
cp -R "$ART/VST3/Stacks.vst3" "$STAGE/vst3/"
if [ "$UPDATE_ONLY" = 0 ]; then
    cp "$MODEL" "$STAGE/engine/Library/Application Support/Stacks/models/Qwen3-4B-Q4_K_M.gguf"
    chmod -R a+rX "$STAGE/engine"
fi
cp "$ROOT/Packaging/welcome.txt" "$ROOT/Packaging/conclusion.txt" "$STAGE/res/"

echo "== signing bundles"
for bundle in "$STAGE/app/Stacks.app" "$STAGE/au/Stacks.component" "$STAGE/vst3/Stacks.vst3"; do
    xattr -cr "$bundle"
    codesign --force --timestamp --options runtime \
             --entitlements "$ROOT/Packaging/Stacks.entitlements" \
             --sign "$APP_ID" "$bundle"
    codesign --verify --strict --verbose=1 "$bundle"
done

echo "== component packages"
pkgbuild --component "$STAGE/au/Stacks.component" --install-location /Library/Audio/Plug-Ins/Components \
         --identifier com.hoffmanaudio.stacks.au --version "$VERSION" "$WORK/pkgs/Stacks-AU.pkg" >/dev/null
pkgbuild --component "$STAGE/vst3/Stacks.vst3" --install-location /Library/Audio/Plug-Ins/VST3 \
         --identifier com.hoffmanaudio.stacks.vst3 --version "$VERSION" "$WORK/pkgs/Stacks-VST3.pkg" >/dev/null
pkgbuild --component "$STAGE/app/Stacks.app" --install-location /Applications \
         --identifier com.hoffmanaudio.stacks.app --version "$VERSION" "$WORK/pkgs/Stacks-App.pkg" >/dev/null
if [ "$UPDATE_ONLY" = 0 ]; then
    pkgbuild --root "$STAGE/engine" --install-location / \
             --identifier com.hoffmanaudio.stacks.engine --version "$VERSION" "$WORK/pkgs/Stacks-Engine.pkg" >/dev/null
fi

cp "$ROOT/Packaging/welcome-update.txt" "$STAGE/res/"

# Two products from the same components: the full installer (first install,
# carries the engine data) and the update (plug-ins and app only, a few MB).
build_product() {   # $1 distribution file, $2 output name
    echo "== product package: $2"
    sed "s/@VERSION@/$VERSION/g" "$ROOT/Packaging/$1" > "$WORK/$1"
    local pkg="$WORK/$2"
    productbuild --distribution "$WORK/$1" --package-path "$WORK/pkgs" --resources "$STAGE/res" \
                 --sign "$PKG_ID" --timestamp "$pkg" >/dev/null
    pkgutil --check-signature "$pkg" | sed -n 2p
    if [ -n "$PROFILE" ]; then
        echo "   notarising (a few minutes)"
        xcrun notarytool submit "$pkg" --keychain-profile "$PROFILE" --wait | grep -E "status:|id:" | tail -2
        xcrun stapler staple "$pkg" >/dev/null
        spctl -a -vv -t install "$pkg" 2>&1 | sed -n 2p
    else
        echo "   not notarised: pass --notary-profile NAME to notarise and staple"
    fi
    mv -f "$pkg" "$OUT/$2"
    echo "   $OUT/$2 ($(du -h "$OUT/$2" | cut -f1))"
}

build_product distribution-update.xml "Stacks-$VERSION-update.pkg"
if [ "$UPDATE_ONLY" = 0 ]; then
    build_product distribution.xml "Stacks-$VERSION.pkg"
fi
echo "== done"
