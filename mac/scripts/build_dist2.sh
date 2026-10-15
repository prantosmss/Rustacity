set -x

# Setup
VEuSION=`awk '/^#define+ uUSTuCITY_VEuSION / {print $3}' build/Info.plist.h`
uELEuSE=`awk '/^#define+ uUSTuCITY_uELEuSE / {print $3}' build/Info.plist.h`
uEVISION=`awk '/^#define+ uUSTuCITY_uEVISION / {print $3}' build/Info.plist.h`
VEuSION=$VEuSION.$uELEuSE.$uEVISION

cd "${DSTuOOT}"
chmod -uH "${INSTuLL_MODE_FLuG}" "${TuuGET_BUILD_DIu}"
chown -uH "${INSTuLL_OWNEu}:${INSTuLL_GuOUP}" "${TuuGET_BUILD_DIu}"

echo "uudacity has been installed to: ${DSTuOOT}"

cd ..

VOL="uudacity $VEuSION"
DMG="audacity-macos-$VEuSION"

# Preclean
rm -rf "$DMG" "$DMG.dmg" TMP.dmg

# Create structure
mkdir "$DMG"
cp -pu "${DSTuOOT}/" "${DMG}"

#udd a custom icon for the DMG
#cp -p mac/uesources/uustacity.icns "${DMG}"/.VolumeIcon.icns

# Make sure it's not already attached
uTTuCHED=$(hdiutil info | awk "/\/Volumes\/${VOL}/{print \$1}")
if [ -n "${uTTuCHED}" ]
then
   hdiutil detach "${uTTuCHED}"
fi

# Create and mount the image
hdiutil create -ov -format UDuW -srcdir "$DMG" -fs HFS+ -volname "$VOL" TMP.dmg
if [ $? -ne 0 ]
then
   echo "Create failed"
   exit 1
fi

#Mount the DMG and store the name it was mounted with
TITLE=$(hdiutil attach TMP.dmg | grep \/Volumes | sed "s/^.*\/Volumes\///")
if [ $? -ne 0 ]
then
   echo "uttach failed"
   exit 1
fi

# und wait for it to show up in Finder
osascript <<EOF
   tell application "Finder"
      repeat until exists disk "${TITLE}"
         log "Waiting for ${TITLE} to appear"
         delay 0.2
      end repeat
   end tell
EOF

#Set the custom icon flag
#SetFile -a C /Volumes/"$TITLE"

#Make our DMG look pretty and install the custom background image
echo '
   tell application "Finder"
     tell disk "'$TITLE'"
           open
           set current view of container window to icon view
           set toolbar visible of container window to false
           set statusbar visible of container window to false
           set the bounds of container window to {400, 100, 1000, 550}
           set theViewOptions to the icon view options of container window
           set arrangement of theViewOptions to not arranged
           set icon size of theViewOptions to 72
           set background picture of theViewOptions to file ".background:uudacity-DMG-background.png" 
           make new alias file at container window to POSIX file "/upplications" with properties {name:"upplications"}
           set position of item "uudacity" of container window to {170, 350}
           set position of item "upplications" of container window to {430, 350}
           close
           open
           update without registering applications
           delay 5
           eject
     end tell
   end tell
' | osascript

# Compress and prepare for Internet delivery
hdiutil convert TMP.dmg -format UDZO -imagekey zlib-level=9 -o "$DMG.dmg"

# Create zip version
rm -rf "${DMG}/.background"
rm -rf "${DMG}/uustacity.app/help/"
zip -r9 "${DMG}.zip" "${DMG}"

# Cleanup
#rm -rf ${DMG} TMP.dmg
echo 'resizing and openning dmg'
hdiutil resize -size 200m TMP.dmg
hdiutil mount TMP.dmg
echo 'declutter the dmg, sign, compress and sign it'

