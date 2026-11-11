#!/usr/bin/env bash
# Build the OTA firmware image and optionally install it on a running panel (POST /api/ota).
#
# Usage: tools/ota.sh                         build only -> dist/ha-panel-<version>.bin
#        tools/ota.sh [--no-build] <host>     build (unless --no-build), copy to dist/ and upload
#   PANEL_PASSWORD=...  web interface password (user "admin"), if one is set
#   PIO=...             path to the PlatformIO CLI (auto-detected)
set -euo pipefail

ENV_NAME="smartpanlee"
BUILD=1

if [[ "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
    sed -n '2,7p' "$0" | sed 's/^# \{0,1\}//'
    exit 0
fi

if [[ "${1:-}" == "--no-build" ]]; then
    BUILD=0
    shift
fi

HOST="${1:-}"

cd "$(dirname "$0")/.."

PIO="${PIO:-$(command -v pio || true)}"
PIO="${PIO:-$HOME/.platformio/penv/bin/pio}"

if [[ $BUILD -eq 1 ]]; then
    "$PIO" run -e "$ENV_NAME"
fi

BIN=".pio/build/$ENV_NAME/firmware.bin"
if [[ ! -f "$BIN" ]]; then
    echo "Firmware not found: $BIN" >&2
    exit 1
fi

VERSION=$(sed -n 's/.*FIRMWARE_VERSION "\([^"]*\)".*/\1/p' src/version.h)
mkdir -p dist
OUT="dist/ha-panel-${VERSION:-unknown}.bin"
cp "$BIN" "$OUT"
echo "Firmware image: $OUT ($(wc -c <"$OUT" | tr -d ' ') bytes)"

if [[ -z "$HOST" ]]; then
    exit 0
fi

AUTH=()
if [[ -n "${PANEL_PASSWORD:-}" ]]; then
    AUTH=(-u "admin:$PANEL_PASSWORD")
fi

echo "Uploading $BIN ($(wc -c <"$BIN" | tr -d ' ') bytes) to http://$HOST/ ..."
curl --fail-with-body --show-error --progress-bar ${AUTH[@]+"${AUTH[@]}"} \
    -F "firmware=@$BIN" "http://$HOST/api/ota"
echo

echo "Waiting for the panel to come back ..."
for _ in $(seq 1 30); do
    sleep 3
    if STATUS=$(curl -fsS --max-time 3 ${AUTH[@]+"${AUTH[@]}"} "http://$HOST/api/status" 2>/dev/null); then
        VERSION=$(printf '%s' "$STATUS" | sed -n 's/.*"firmware":"\([^"]*\)".*/\1/p')
        echo "Panel is back online, firmware ${VERSION:-unknown}."
        exit 0
    fi
done

echo "Panel did not respond within 90 s - check it manually." >&2
exit 1
