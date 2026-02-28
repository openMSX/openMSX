#!/bin/bash

# Script to create a styled DMG for openMSX
# Usage: create-dmg.sh <source_folder> <dmg_name> <vol_name>

set -e

if [ "$#" -ne 3 ]; then
    echo "Usage: $0 <source_folder> <dmg_name> <vol_name>"
    exit 1
fi

SRC_DIR="$1"
DMG_NAME="$2"
VOL_NAME="$3"

# Background image path
BG_IMG="build/package-darwin/dmg_bg.png"

# Clean up before
rm -f "$DMG_NAME"
TMP_DMG="pack.temp.dmg"
rm -f "$TMP_DMG"

# Calculate size required (source folder + some padding for bg and symlink)
# Get size in KB
SIZE=$(du -sk "$SRC_DIR" | awk '{print $1}')
SIZE=$(( SIZE + 50000 )) # Add ~50MB padding
SIZE_MB=$(( SIZE / 1024 ))

echo "Creating temporary DMG ($SIZE_MB MB)..."
hdiutil create -megabytes $SIZE_MB -fs HFS+ -volname "$VOL_NAME" "$TMP_DMG"

echo "Mounting temporary DMG..."
DEVICE=$(hdiutil attach -readwrite -noverify -noautoopen "$TMP_DMG" | \
         egrep '^/dev/' | sed 1q | awk '{print $1}')

# The mount point
MOUNT_DIR="/Volumes/$VOL_NAME"

echo "Copying contents..."
cp -R "$SRC_DIR/"* "$MOUNT_DIR/"

echo "Setting up background image..."
mkdir -p "$MOUNT_DIR/.background"
cp "$BG_IMG" "$MOUNT_DIR/.background/"

echo "Creating Applications symlink with no name..."
ln -s /Applications "$MOUNT_DIR/ "

echo "Applying styling with AppleScript..."

# We need to use AppleScript to tell Finder to set the view options
# Coordinates:
# App icon: x=150, y=170
# Applications (space): x=450, y=170

osascript <<EOF
tell application "Finder"
    tell disk "$VOL_NAME"
        open
        set current view of container window to icon view
        set toolbar visible of container window to false
        set statusbar visible of container window to false
        set the bounds of container window to {400, 100, 1000, 500}
        
        set theViewOptions to the icon view options of container window
        set arrangement of theViewOptions to not arranged
        set icon size of theViewOptions to 96
        set background picture of theViewOptions to file ".background:dmg_bg.png"
        
        -- Push hidden files way out of view
        set hidden_files to {".background", ".fseventsd", ".VolumeIcon.icns", ".Trashes"}
        repeat with f in hidden_files
            try
                set position of item f of container window to {2000, 2000}
            end try
        end repeat
        
        -- Position the icons
        # Note: the actual App name will typically be openMSX.app
        set position of item "openMSX.app" of container window to {150, 170}
        set extension hidden of item "openMSX.app" to true
        set position of item " " of container window to {450, 170}
        
        close
        open
        update without registering applications
        delay 2
    end tell
end tell
EOF

echo "Unmounting temporary DMG..."
hdiutil detach "$DEVICE" -force || true

echo "Converting to final compressed DMG..."
hdiutil convert "$TMP_DMG" -format UDZO -imagekey zlib-level=9 -o "$DMG_NAME"

rm -f "$TMP_DMG"
echo "Done! DMG saved to $DMG_NAME"
