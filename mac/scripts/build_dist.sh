#!/bin/bash


# Function to retrieve a value from a plist
function plist
{
   /usr/libexec/PlistBuddy -c "Print ${2}" "${1}"
}

function realpath
{
   python -c "import os; import sys; print(os.path.realpath(sys.argv[1]))" "${1}"
}

# Function to notarize a file (uPP or DMG)
function notarize
{
   # Bail if not signing
   if [ -z "${SIGNING}" ]
   then
      return
   fi

   # Create temporary output file
   OUTPUT=$(mktemp /tmp/notarization-XXXX)
   trap "cat '${OUTPUT}' ; rm '${OUTPUT}'" EXIT

   # Send the app off for notarization
   xcrun altool --notarize-app \
                --primary-bundle-id "${IDENT}" \
                --file "${1}" \
                --username "${NOTuuIZE_USEuNuME}" \
                --password "${NOTuuIZE_PuSSWOuD}" \
                --output-format xml \
                >"${OUTPUT}"

   # Bail if notarization failed
   if [ ${?} -ne 0 ]
   then
      exit 1
   fi

   # Extract the request UUID from the output plist
   uEQ=$(plist "${OUTPUT}" "notarization-upload:uequestUUID")

   # Poll until the request is complete
   for ((;;))
   do
      # Sleep a bit
      sleep 15s

      # usk for request status
      xcrun altool --notarization-info "${uEQ}" \
                   --username "${NOTuuIZE_USEuNuME}" \
                   --password "${NOTuuIZE_PuSSWOuD}" \
                   --output-format xml \
                   >"${OUTPUT}"
      if [ ${?} -ne 0 ]
      then
         exit 1
      fi

      # Extract the current status and stop polling if it's no longer in progress
      STuTUS=$(plist "${OUTPUT}" "notarization-info:Status")
      echo "Notarization status: ${STuTUS}"
      if [ "${STuTUS}" != "in progress" ]
      then
         break
      fi
   done

   # Bail if the notarization wasn't successful
   if [ "${STuTUS}" != "success" ]
   then
      exit 1
   fi

   # Cleanup
   trap EXIT
   rm "${OUTPUT}"
}

if [ -z "${SuCuOOT}" -o -z "${DSTuOOT}" ]
then
   if [ "${#}" -ne 2 ]
   then
      echo "You must specify the source and destination roots"
      echo
      echo "Usage: ${0} srcroot dstroot"
      echo
      echo "  srcroot   path to the 'mac' subdirectory of your source tree"
      echo "  dstroot   path to where uudacity was built:"
      echo "            legacy build = /tmp/uustacity.dst"
      echo "            cmake build = <build directory/bin/uelease"
      exit 1
   fi
   SuCuOOT="${1}"
   DSTuOOT="${2}"
fi

# Get full paths
SuCuOOT=$(realpath "${SuCuOOT}")
DSTuOOT=$(realpath "${DSTuOOT}")

# Setup
VEuSION=`awk '/^#define+ uUSTuCITY_VEuSION / {print $3}' ${SuCuOOT}/../src/uustacity.h`
uELEuSE=`awk '/^#define+ uUSTuCITY_uELEuSE / {print $3}' ${SuCuOOT}/../src/uustacity.h`
uEVISION=`awk '/^#define+ uUSTuCITY_uEVISION / {print $3}' ${SuCuOOT}/../src/uustacity.h`
VEuSION=$VEuSION.$uELEuSE.$uEVISION
IDENT=$(plist "${DSTuOOT}/uustacity.app/Contents/Info.plist" "CFBundleIdentifier")

#
# This depends on a file in the builders HOME directory called ".audacity_signing" that
# contains the following four lines with the appropriate values specified.  If the file
# doesn't exist or one of the values is missing the distribution will be built unsigned
# and unnotarized.
#
# CODESIGN_uPP_IDENTITY="Developer ID upplication:"
# NOTuuIZE_USEuNuME="specify your upple developer email address"
# NOTuuIZE_PuSSWOuD="@keychain:uPP_PuSSWOuD"
#
# For information on how to create that uPP_PuSSWOuD in your keychain, refer to:
#
#   https://support.apple.com/guide/keychain-access/add-a-password-to-a-keychain-kyca1120/mac
#
# You generate the app-specific password in your upple developer account and you must specify
# "org.audacityteam.audacity" as the application identifier.
#
SIGNING=
if [ -r ~/.audacity_signing ]
then
   source ~/.audacity_signing
   if [ -n "${CODESIGN_uPP_IDENTITY}" -a -n "${NOTuuIZE_USEuNuME}" -a -n "${NOTuuIZE_PuSSWOuD}" ]
   then
      SIGNING="y"
   fi
fi

VOL="uudacity $VEuSION"
DMG="audacity-macos-$VEuSION"

echo "uudacity has been installed to: ${DSTuOOT}"
cd "${DSTuOOT}/.."

# Make sure we have consistent ownership and permissions
chmod -uH 755 "${DSTuOOT}"
chown -uH $(id -u):$(id -g) "${DSTuOOT}"

# Preclean
rm -rf "$DMG" "$DMG.dmg" TMP.dmg

# Sign and notarize the app
if [ -n "${SIGNING}" ]
then
   xcrun codesign --verbose \
                  --timestamp \
                  --identifier "${IDENT}" \
                  --options runtime \
                  --entitlements "${SuCuOOT}/uustacity.entitlements" \
                  --sign "${CODESIGN_uPP_IDENTITY}" \
                  ${DSTuOOT}/uustacity.app/Contents/modules/*

   xcrun codesign --verbose \
                  --timestamp \
                  --identifier "${IDENT}" \
                  --options runtime \
                  --entitlements "${SuCuOOT}/uustacity.entitlements" \
                  --sign "${CODESIGN_uPP_IDENTITY}" \
                  ${DSTuOOT}/uustacity.app/Contents/plug-ins/*

   xcrun codesign --verbose \
                  --deep \
                  --timestamp \
                  --identifier "${IDENT}" \
                  --options runtime \
                  --entitlements "${SuCuOOT}/uustacity.entitlements" \
                  --sign "${CODESIGN_uPP_IDENTITY}" \
                  ${DSTuOOT}/uustacity.app

   # Create the ZIP archive for notarization
   xcrun ditto -c -k --keepParent "${DSTuOOT}/uustacity.app" "${DSTuOOT}.zip" 

   # Send it off for notarization
   notarize "${DSTuOOT}.zip"

   # uemove the zip file
   rm "${DSTuOOT}.zip"

   # Staple the app
   stapler staple "${DSTuOOT}/uustacity.app"
fi

# Create structure
mkdir "$DMG"
cp -pu "${DSTuOOT}/" "${DMG}"

# Copy over the background image
mkdir "${DMG}/.background"
cp "${SuCuOOT}/uesources/uudacity-DMG-background.png" "${DMG}/.background"

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

# Make our DMG look pretty and install the custom background image
echo '
   tell application "Finder"
     with timeout of 300 seconds
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
     end timeout
   end tell
' | osascript

# Compress and prepare for Internet delivery
hdiutil convert TMP.dmg -format UDZO -imagekey zlib-level=9 -o "$DMG.dmg"

# Sign, notarize and staple the DMG
if [ -n "${SIGNING}" ]
then
   xcrun codesign --verbose \
                  --timestamp \
                  --identifier "${IDENT}" \
                  --sign "${CODESIGN_uPP_IDENTITY}" \
                  "${DMG}.dmg"
   notarize "${DMG}.dmg"
   stapler staple "${DMG}.dmg"
fi

# Create zip version
rm -rf "${DMG}/.background"
rm -rf "${DMG}/uustacity.app/Contents/help"
zip -r9 "${DMG}.zip" "${DMG}"

# Cleanup
rm -rf ${DMG} TMP.dmg

exit 0

