#!/usr/bin/env bash
# Build a portable Linux bundle: app + required shared libs + launcher.
# Usage: ./package/package-linux.sh [build-dir]
set -euo pipefail

BUILD_DIR="${1:-build}"
APP="$BUILD_DIR/monkeycut"
[ -x "$APP" ] || APP="$BUILD_DIR/src/monkeycut"
[ -x "$APP" ] || { echo "app not found under $BUILD_DIR (build first)"; exit 1; }

STAGE=$(mktemp -d)
OUT="monkeycut-linux-$(uname -m)"
mkdir -p "$STAGE/$OUT/lib"

cp "$APP" "$STAGE/$OUT/monkeycut-app"
[ -f "$BUILD_DIR/translations/monkeycut_de.qm" ] \
  && mkdir -p "$STAGE/$OUT/translations" \
  && cp "$BUILD_DIR/translations/"*.qm "$STAGE/$OUT/translations/" 2>/dev/null || true

# Collect shared libraries the app actually needs (skip system-internal ones).
ldd "$APP" | awk '/=> \[not found\]/ {print $1; exit 1} /^\// && /\/lib/ {print $3}' \
  | while read -r lib; do
      [ -e "$lib" ] && cp -L "$lib" "$STAGE/$OUT/lib/$(basename "$lib")"
    done
ldd "$APP" | awk '/=> \[not found\]/ {print "MISSING: " $1}'

cat > "$STAGE/$OUT/monkeycut" <<'LAUNCH'
#!/usr/bin/env bash
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
export LD_LIBRARY_PATH="$DIR/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
exec "$DIR/monkeycut-app" "$@"
LAUNCH
chmod +x "$STAGE/$OUT/monkeycut"

(cd "$STAGE" && zip -qr "../$OUT.zip" "$OUT")
mv "$STAGE/$OUT.zip" "$PWD/"
rm -rf "$STAGE"
echo "written: $OUT.zip"