#!/usr/bin/env bash

set -euo pipefail

usage() {
    echo "Usage: $0 <path-to-5250ng.app> <output.dmg>" >&2
    echo >&2
    echo "Optional environment variables:" >&2
    echo "  MACOS_SIGNING_IDENTITY  Developer ID Application identity" >&2
    echo "  MACOS_NOTARY_KEY        Path to an App Store Connect API .p8 key" >&2
    echo "  MACOS_NOTARY_KEY_ID     App Store Connect API key ID" >&2
    echo "  MACOS_NOTARY_ISSUER_ID  App Store Connect issuer ID" >&2
    echo "  MACOS_EXPECTED_ARCH     Required executable arch (arm64 or x86_64)" >&2
    echo "  MACOS_EXPECTED_MIN_VERSION  Required deployment target (for example 13.0)" >&2
}

if [[ $# -ne 2 ]]; then
    usage
    exit 2
fi

if [[ "$(uname -s)" != "Darwin" ]]; then
    echo "error: macOS packages must be created on macOS" >&2
    exit 1
fi

app_path="$1"
output_dmg="$2"
app_name="$(basename "$app_path")"

if [[ ! -d "$app_path" || ! -x "$app_path/Contents/MacOS/5250ng" ]]; then
    echo "error: $app_path is not a built 5250ng application bundle" >&2
    exit 1
fi

if [[ -n "${MACOS_EXPECTED_ARCH:-}" ]]; then
    if ! lipo "$app_path/Contents/MacOS/5250ng" \
        -verify_arch "$MACOS_EXPECTED_ARCH"; then
        echo "error: application does not contain $MACOS_EXPECTED_ARCH code" >&2
        lipo -archs "$app_path/Contents/MacOS/5250ng" >&2 || true
        exit 1
    fi
fi

if [[ -n "${MACOS_EXPECTED_MIN_VERSION:-}" ]]; then
    minimum_version="$(otool -l "$app_path/Contents/MacOS/5250ng" \
        | awk '/LC_BUILD_VERSION/{build=1} build && /minos/{print $2; exit}')"
    if [[ "$minimum_version" != "$MACOS_EXPECTED_MIN_VERSION" ]]; then
        echo "error: expected macOS deployment target $MACOS_EXPECTED_MIN_VERSION, found $minimum_version" >&2
        exit 1
    fi
fi

macdeployqt_bin="${MACDEPLOYQT:-$(command -v macdeployqt || true)}"
if [[ -z "$macdeployqt_bin" ]] && command -v brew >/dev/null 2>&1; then
    homebrew_macdeployqt="$(brew --prefix qt@6 2>/dev/null || true)/bin/macdeployqt"
    if [[ -x "$homebrew_macdeployqt" ]]; then
        macdeployqt_bin="$homebrew_macdeployqt"
    fi
fi
if [[ -z "$macdeployqt_bin" ]]; then
    echo "error: macdeployqt was not found; add the Qt bin directory to PATH" >&2
    exit 1
fi

mkdir -p "$(dirname "$output_dmg")"

echo "Deploying Qt frameworks and plugins into $app_path"
deploy_options=(-always-overwrite -verbose=1)
if [[ -n "${MACOS_SIGNING_IDENTITY:-}" ]]; then
    echo "Signing the application with $MACOS_SIGNING_IDENTITY"
    deploy_options+=("-sign-for-notarization=$MACOS_SIGNING_IDENTITY")
else
    echo "No Developer ID identity supplied; applying an ad-hoc signature"
    deploy_options+=("-codesign=-")
fi
"$macdeployqt_bin" "$app_path" "${deploy_options[@]}"

plutil -lint "$app_path/Contents/Info.plist"
codesign --verify --deep --strict --verbose=2 "$app_path"

echo "Checking the bundle for non-system absolute library dependencies"
while IFS= read -r -d '' candidate; do
    if ! file -b "$candidate" | grep -q 'Mach-O'; then
        continue
    fi

    if otool -L "$candidate" | grep -q '/AGL.framework/'; then
        echo "error: obsolete AGL framework dependency found in $candidate" >&2
        exit 1
    fi

    invalid_dependencies="$({ otool -L "$candidate" || true; } \
        | tail -n +2 \
        | awk '{print $1}' \
        | grep -E '^/' \
        | grep -Ev '^(/System/Library/|/usr/lib/)' || true)"
    if [[ -n "$invalid_dependencies" ]]; then
        echo "error: $candidate contains non-portable dependencies:" >&2
        echo "$invalid_dependencies" >&2
        exit 1
    fi
done < <(find "$app_path" -type f -print0)

echo "Running packaged application smoke test"
version_output="$("$app_path/Contents/MacOS/5250ng" --version 2>&1)"
if [[ "$version_output" != *"5250ng"* ]]; then
    echo "error: packaged application did not report its version" >&2
    echo "$version_output" >&2
    exit 1
fi

staging_dir="$(mktemp -d "${TMPDIR:-/tmp}/5250ng-dmg.XXXXXX")"
mount_dir="$(mktemp -d "${TMPDIR:-/tmp}/5250ng-dmg-mount.XXXXXX")"
dmg_mounted=false
cleanup() {
    if [[ "$dmg_mounted" == true ]]; then
        hdiutil detach "$mount_dir" >/dev/null 2>&1 || true
    fi
    rm -rf -- "${staging_dir:?}"
    rm -rf -- "${mount_dir:?}"
}
trap cleanup EXIT

ditto "$app_path" "$staging_dir/$app_name"
ln -s /Applications "$staging_dir/Applications"

echo "Creating $output_dmg"
hdiutil create \
    -volname "5250ng" \
    -srcfolder "$staging_dir" \
    -format UDZO \
    -ov \
    "$output_dmg"
hdiutil verify "$output_dmg"

echo "Verifying the application inside the final disk image"
hdiutil attach -nobrowse -readonly -mountpoint "$mount_dir" "$output_dmg" >/dev/null
dmg_mounted=true
packaged_app="$mount_dir/$app_name"
test -L "$mount_dir/Applications"
test -L "$packaged_app/Contents/Frameworks/QtCore.framework/Versions/Current"
if [[ -n "${MACOS_EXPECTED_ARCH:-}" ]]; then
    lipo "$packaged_app/Contents/MacOS/5250ng" \
        -verify_arch "$MACOS_EXPECTED_ARCH"
fi
codesign --verify --deep --strict --verbose=2 "$packaged_app"
"$packaged_app/Contents/MacOS/5250ng" --version
hdiutil detach "$mount_dir" >/dev/null
dmg_mounted=false

if [[ -n "${MACOS_SIGNING_IDENTITY:-}" ]]; then
    codesign --force --timestamp --sign "$MACOS_SIGNING_IDENTITY" "$output_dmg"
    codesign --verify --verbose=2 "$output_dmg"
fi

notary_values=(
    "${MACOS_NOTARY_KEY:-}"
    "${MACOS_NOTARY_KEY_ID:-}"
    "${MACOS_NOTARY_ISSUER_ID:-}"
)
notary_value_count=0
for value in "${notary_values[@]}"; do
    [[ -n "$value" ]] && ((notary_value_count += 1))
done

if [[ $notary_value_count -ne 0 && $notary_value_count -ne 3 ]]; then
    echo "error: all three MACOS_NOTARY_* values are required for notarization" >&2
    exit 1
fi

if [[ $notary_value_count -eq 3 ]]; then
    if [[ -z "${MACOS_SIGNING_IDENTITY:-}" ]]; then
        echo "error: notarization requires a Developer ID signature" >&2
        exit 1
    fi
    if [[ ! -f "$MACOS_NOTARY_KEY" ]]; then
        echo "error: notarization key not found: $MACOS_NOTARY_KEY" >&2
        exit 1
    fi

    echo "Submitting the disk image to Apple's notary service"
    xcrun notarytool submit "$output_dmg" \
        --key "$MACOS_NOTARY_KEY" \
        --key-id "$MACOS_NOTARY_KEY_ID" \
        --issuer "$MACOS_NOTARY_ISSUER_ID" \
        --wait
    xcrun stapler staple "$output_dmg"
    xcrun stapler validate "$output_dmg"
    spctl --assess --type open --context context:primary-signature \
        --verbose=2 "$output_dmg"
else
    echo "Notarization credentials were not supplied; the DMG is not notarized"
fi

echo "Created macOS installer: $output_dmg"
