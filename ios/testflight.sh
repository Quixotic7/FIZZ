#!/bin/sh
# SPDX-License-Identifier: MPL-2.0
# This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
# distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
#
# Archives FIZZ for iOS (Release) and uploads it to App Store Connect for TestFlight.
#
#   ios/testflight.sh [archive|upload] [BUILD_NUMBER]
#
# archive: only the archive (checks signing). upload (the default): the archive, then the upload.
# BUILD_NUMBER defaults to the time (yyyymmdd.hhmm), so every upload is higher than the last.
# INTERNAL_ONLY=1 marks the build for internal testers only.
#
# Needs: the team in ios/Local.xcconfig (DEVELOPMENT_TEAM = ..., git ignores it), the Apple ID of that team signed in
# to Xcode (Settings > Accounts; Xcode's automatic signing makes the App Store profile), and the app's record in App
# Store Connect for com.quixotic7.fizz.
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
MODE=${1:-upload}
BUILD=${2:-$(date +%Y%m%d.%H%M)}
TEAM=$(sed -n 's/^[[:space:]]*DEVELOPMENT_TEAM[[:space:]]*=[[:space:]]*\([A-Z0-9]\{10\}\).*/\1/p' "$HERE/Local.xcconfig" 2>/dev/null | head -1)
[ -n "$TEAM" ] || { echo "no team: cp ios/Local.xcconfig.example ios/Local.xcconfig and set DEVELOPMENT_TEAM"; exit 1; }
OUT=${OUT:-${TMPDIR:-/tmp}/fizz-testflight/$BUILD}
INTERNAL=false
[ "${INTERNAL_ONLY:-0}" = 1 ] && INTERNAL=true

# Homebrew's rsync 3.x breaks Xcode's .ipa step (Apple's rsync runs the first rsync on PATH as its server).
PATH=/usr/bin:/bin:/usr/sbin:/sbin
export PATH

mkdir -p "$OUT"
cat > "$OUT/ExportOptions.plist" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
	<key>method</key><string>app-store-connect</string>
	<key>destination</key><string>upload</string>
	<key>teamID</key><string>$TEAM</string>
	<key>signingStyle</key><string>automatic</string>
	<key>uploadSymbols</key><true/>
	<key>manageAppVersionAndBuildNumber</key><false/>
	<key>testFlightInternalTestingOnly</key><$INTERNAL/>
</dict>
</plist>
EOF

echo "FIZZ build $BUILD -> $OUT"
xcodebuild -project "$HERE/FIZZ.xcodeproj" -scheme FIZZ -configuration Release \
    -destination 'generic/platform=iOS' -archivePath "$OUT/FIZZ.xcarchive" \
    -derivedDataPath "$OUT/dd" -allowProvisioningUpdates \
    CURRENT_PROJECT_VERSION="$BUILD" archive

APP="$OUT/FIZZ.xcarchive/Products/Applications/FIZZ.app"
/usr/libexec/PlistBuddy -c 'Print :CFBundleShortVersionString' -c 'Print :CFBundleVersion' "$APP/Info.plist"
[ -f "$APP/PrivacyInfo.xcprivacy" ] || { echo "error: PrivacyInfo.xcprivacy is not in the app"; exit 1; }
[ "$MODE" = archive ] && { echo "archived: $OUT/FIZZ.xcarchive"; exit 0; }

xcodebuild -exportArchive -archivePath "$OUT/FIZZ.xcarchive" -exportOptionsPlist "$OUT/ExportOptions.plist" \
    -exportPath "$OUT/export" -allowProvisioningUpdates
echo "uploaded build $BUILD: App Store Connect > FIZZ > TestFlight (processing takes a few minutes)"
