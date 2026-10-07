#!/usr/bin/env bash
# package_macos.sh STAGE OUT VERSION -- sign, then package, the macOS bundles (B446 Wave 2).
#
# Called by ci.yml's package-macos (main) and sign-macos (v* tags) jobs. Those jobs
# run NO third-party tool: they download the bundles the build job tarred, check
# the tar against the hash the build job published, and call this. pluginval runs
# in its own job, whose success gates them.
#
# STAGE holds VST3/horde.vst3, Components/horde.component and CLAP/horde.clap.
# OUT receives exactly two files: horde-macos-universal-VERSION.zip and .pkg.
#
# ORDER IS THE POINT: every bundle is signed (and, with credentials, notarized and
# stapled) BEFORE the zip and the pkg are built from it, so what ships is what was
# signed. The previous workflow zipped the ad-hoc bundles and then re-signed only
# the pkg's copy.
#
# SIGNING MODE comes from the environment, all or nothing:
#   none of APPLE_ID, APPLE_TEAM_ID, APPLE_APP_PASSWORD, DEVELOPER_ID_CERT_P12,
#   DEVELOPER_ID_CERT_PASSWORD set  -> ad-hoc seal, said plainly (no secrets exist yet);
#   all five set                    -> Developer ID sign + notarize + staple;
#   some but not all                -> RED: a half-configured signer is a mistake,
#                                      not a reason to ship ad-hoc quietly.
# The mode is printed as `signing=<mode>` on the last line and, under Actions,
# written to $GITHUB_OUTPUT so the release notes can say which one shipped.
#
# The keychain gets a random password, the key is usable by codesign and
# productsign only, and the decoded certificate and the keychain are deleted on
# exit, whatever the exit path.
set -euo pipefail

die() { echo "package_macos: $*" >&2; exit 1; }
[ $# -eq 3 ] || die "usage: package_macos.sh STAGE OUT VERSION"
STAGE=$1 OUT=$2 VERSION=$3
case "$VERSION" in ''|*/*|*' '*) die "bad VERSION '$VERSION'";; esac
BUNDLES=(VST3/horde.vst3 Components/horde.component CLAP/horde.clap)
for b in "${BUNDLES[@]}"; do test -e "$STAGE/$b" || die "missing $STAGE/$b"; done

# Never package into the source tree: a tracked directory swept into an upload is
# how a committed binary once rode into every artifact.
mkdir -p "$OUT"
out_real=$(cd "$OUT" && pwd -P)
here=$(cd "$(dirname "$0")/.." && pwd -P)
case "$out_real/" in "$here"/*) die "OUT ($OUT) is inside the source tree; package under \$RUNNER_TEMP";; esac
[ -z "$(ls -A "$OUT")" ] || die "OUT ($OUT) is not empty"

CREDS=(APPLE_ID APPLE_TEAM_ID APPLE_APP_PASSWORD DEVELOPER_ID_CERT_P12 DEVELOPER_ID_CERT_PASSWORD)
n=0
for c in "${CREDS[@]}"; do [ -n "${!c:-}" ] && n=$((n + 1)); done
if [ "$n" -eq 0 ]; then
  MODE=adhoc
  echo "package_macos: Apple signing credentials absent; bundles are AD-HOC sealed, not notarized"
elif [ "$n" -eq "${#CREDS[@]}" ]; then
  MODE=developer-id
else
  die "$n of ${#CREDS[@]} Apple signing credentials are set; set all of them or none"
fi

TMP=$(mktemp -d "${RUNNER_TEMP:-${TMPDIR:-/tmp}}/package_macos.XXXXXX")
KEYCHAIN=""
cleanup() {
  [ -n "$KEYCHAIN" ] && security delete-keychain "$KEYCHAIN" >/dev/null 2>&1 || true
  rm -rf "$TMP"
}
trap cleanup EXIT

notarize() {  # notarize FILE: submit, wait, and require status Accepted.
  local result status
  result=$(xcrun notarytool submit "$1" --apple-id "$APPLE_ID" --team-id "$APPLE_TEAM_ID" \
             --password "$APPLE_APP_PASSWORD" --wait --output-format json)
  status=$(printf '%s' "$result" | python3 -c 'import json,sys; print(json.load(sys.stdin).get("status",""))')
  [ "$status" = "Accepted" ] || die "notarization of $(basename "$1") returned '$status'"
}

if [ "$MODE" = developer-id ]; then
  KEYCHAIN="$TMP/signing.keychain-db"
  kc_pw=$(openssl rand -hex 32)
  security create-keychain -p "$kc_pw" "$KEYCHAIN"
  security set-keychain-settings -lut 3600 "$KEYCHAIN"
  security unlock-keychain -p "$kc_pw" "$KEYCHAIN"
  p12="$TMP/cert.p12"
  printf '%s' "$DEVELOPER_ID_CERT_P12" | base64 --decode > "$p12"
  security import "$p12" -k "$KEYCHAIN" -f pkcs12 -P "$DEVELOPER_ID_CERT_PASSWORD" \
           -T /usr/bin/codesign -T /usr/bin/productsign
  rm -P "$p12"
  security set-key-partition-list -S apple-tool:,apple: -s -k "$kc_pw" "$KEYCHAIN" >/dev/null
  # Search list = the signing keychain first, then whatever was already there.
  existing=$(security list-keychains -d user | sed 's/^[[:space:]]*"//; s/"[[:space:]]*$//')
  # shellcheck disable=SC2086  # word-splitting the existing list is intended
  security list-keychains -d user -s "$KEYCHAIN" $existing
  for b in "${BUNDLES[@]}"; do
    codesign --force --deep --timestamp --options runtime --keychain "$KEYCHAIN" \
             -s "Developer ID Application" "$STAGE/$b"
  done
else
  # Sign LAST, after every build step has touched the bundle: a seal broken by a
  # post-sign write is silently skipped by DAW scanners.
  for b in "${BUNDLES[@]}"; do codesign --force --deep -s - "$STAGE/$b"; done
fi
for b in "${BUNDLES[@]}"; do codesign --verify --deep --strict "$STAGE/$b"; done

if [ "$MODE" = developer-id ]; then
  # Notarize the signed bundles themselves and staple each ticket, so the zip
  # below carries notarized bundles, not just the pkg.
  ditto -c -k --sequesterRsrc "$STAGE" "$TMP/bundles.zip"
  notarize "$TMP/bundles.zip"
  for b in "${BUNDLES[@]}"; do
    xcrun stapler staple "$STAGE/$b"
    xcrun stapler validate "$STAGE/$b"
  done
fi

ZIP="$OUT/horde-macos-universal-$VERSION.zip"
PKG="$OUT/horde-macos-universal-$VERSION.pkg"
# ditto keeps bundle permissions and resource forks; upload-artifact's own zip does not.
ditto -c -k --sequesterRsrc "$STAGE" "$ZIP"
if [ "$MODE" = developer-id ]; then
  pkgbuild --root "$STAGE" --install-location /Library/Audio/Plug-Ins \
           --identifier com.lifted-truck.horde --version "$VERSION" "$TMP/unsigned.pkg"
  productsign --sign "Developer ID Installer" --keychain "$KEYCHAIN" "$TMP/unsigned.pkg" "$PKG"
  notarize "$PKG"
  xcrun stapler staple "$PKG"
  xcrun stapler validate "$PKG"
else
  pkgbuild --root "$STAGE" --install-location /Library/Audio/Plug-Ins \
           --identifier com.lifted-truck.horde --version "$VERSION" "$PKG"
fi

[ "$(ls -A "$OUT" | wc -l | tr -d ' ')" -eq 2 ] || die "OUT holds more than the zip and the pkg"
[ -n "${GITHUB_OUTPUT:-}" ] && echo "signing=$MODE" >> "$GITHUB_OUTPUT"
echo "signing=$MODE"
