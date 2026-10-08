#!/bin/bash
# Builds a self-contained Kya.app from the macos-release build and the game data in bin/MAC.
# Saves, settings and logs go to ~/Library/Application Support/Kya, not into the bundle.
# Usage: tools/macos/make_app.sh [output dir, default out/app]
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
OUT="${1:-$ROOT/out/app}"
SDK="${VULKAN_SDK:-$HOME/VulkanSDK/1.4.363.0/macOS}"
BIN="$ROOT/bin/MAC"
APP="$OUT/Kya.app"
DATA=(CDEURO IOP SLES_514.73 SYSTEM.CNF BWITCH.INI shaders)

[ -x "$BIN/Kya_RelWithDebInfo" ] || { echo "Build first: cmake --preset macos-release && cmake --build out/build/macos-release"; exit 1; }
for d in "${DATA[@]}"; do [ -e "$BIN/$d" ] || { echo "Missing game data: $BIN/$d"; exit 1; }; done

rm -rf "$APP"
mkdir -p "$APP/Contents/MacOS" "$APP/Contents/Frameworks" "$APP/Contents/Resources/game" "$APP/Contents/Resources/vulkan/icd.d"

cp "$BIN/Kya_RelWithDebInfo" "$APP/Contents/MacOS/Kya_bin"
cp -L "$SDK/lib/libvulkan.1.dylib" "$SDK/lib/libMoltenVK.dylib" "$APP/Contents/Frameworks/"
install_name_tool -add_rpath "@executable_path/../Frameworks" "$APP/Contents/MacOS/Kya_bin"
cat > "$APP/Contents/Resources/vulkan/icd.d/MoltenVK_icd.json" <<'JSON'
{
    "file_format_version": "1.0.0",
    "ICD": {
        "library_path": "../../../Frameworks/libMoltenVK.dylib",
        "api_version": "1.4.0",
        "is_portability_driver": true
    }
}
JSON

for d in "${DATA[@]}"; do ditto "$BIN/$d" "$APP/Contents/Resources/game/$d"; done

# The game reads and writes relative to its working directory, so run it from a
# per-user folder that links back to the read-only data inside the bundle.
cat > "$APP/Contents/MacOS/Kya" <<'SH'
#!/bin/bash
CONTENTS="$(cd "$(dirname "$0")/.." && pwd)"
HOME_DIR="$HOME/Library/Application Support/Kya"
mkdir -p "$HOME_DIR"
for d in "$CONTENTS/Resources/game/"*; do ln -sfn "$d" "$HOME_DIR/$(basename "$d")"; done
cd "$HOME_DIR"
export VK_ICD_FILENAMES="$CONTENTS/Resources/vulkan/icd.d/MoltenVK_icd.json"
export KYA_HIDE_DEBUG=1
exec "$CONTENTS/MacOS/Kya_bin" "$@" > "$HOME_DIR/last_run.log" 2>&1
SH
chmod +x "$APP/Contents/MacOS/Kya"

cat > "$APP/Contents/Info.plist" <<'PLIST'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
	<key>CFBundleName</key><string>Kya</string>
	<key>CFBundleDisplayName</key><string>Kya: Dark Lineage</string>
	<key>CFBundleIdentifier</key><string>local.kya.port</string>
	<key>CFBundleExecutable</key><string>Kya</string>
	<key>CFBundlePackageType</key><string>APPL</string>
	<key>CFBundleShortVersionString</key><string>0.1</string>
	<key>LSMinimumSystemVersion</key><string>13.0</string>
	<key>LSApplicationCategoryType</key><string>public.app-category.games</string>
	<key>NSHighResolutionCapable</key><true/>
</dict>
</plist>
PLIST

codesign --force --deep --sign - "$APP"
echo "Built $APP"
