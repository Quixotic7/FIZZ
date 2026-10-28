#!/bin/sh
# SPDX-License-Identifier: MPL-2.0
# This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
# distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
#
# FizzKit's, FizzUI's and FizzData's tests on an iOS simulator, headless (Simulator.app stays closed), and the app
# installed and launched on the demo unit.
#
#   ios/sim_test.sh [SIMULATOR_UDID] [DERIVED_DATA_DIR]
#
# The test bundles are built by xcodebuild (build-for-testing) and run with `simctl spawn ... xctest`, not
# `xcodebuild test`: on this Mac Xcode's default toolchain is overridden (DVTDefaultToolchainOverrideIdentifer = a 2024
# Swift snapshot) and xcodebuild test injects that toolchain's Swift runtime into the simulator, where the tests then
# abort at launch (Symbol not found: _swift_cvw_initializeBufferWithCopyOfBuffer).
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
DEV=${1:-$(xcrun simctl list devices booted | sed -n 's/.*iPhone.*(\([0-9A-F-]\{36\}\)) (Booted).*/\1/p' | head -1)}
DD=${2:-${TMPDIR:-/tmp}/fizz-ios-dd}
[ -n "$DEV" ] || { echo "no booted iPhone simulator: xcrun simctl boot <udid>"; exit 1; }

cd "$HERE/FizzKit"
xcodebuild -scheme FizzKit-Package -destination "platform=iOS Simulator,id=$DEV" -derivedDataPath "$DD" \
  build-for-testing -quiet
P="$DD/Build/Products/Debug-iphonesimulator"
X=/Applications/Xcode.app/Contents/Developer/Platforms/iPhoneSimulator.platform/Developer/Library/Xcode/Agents/xctest
for B in FizzKitTests FizzUITests FizzDataTests; do
  echo "== $B"
  SIMCTL_CHILD_DYLD_FRAMEWORK_PATH="$P" SIMCTL_CHILD_DYLD_LIBRARY_PATH="$P" \
    xcrun simctl spawn "$DEV" "$X" "$P/$B.xctest" 2>&1 | grep -E "Test run with|✘"
done

cd "$HERE"
xcodebuild -project FIZZ.xcodeproj -scheme FIZZ -destination "platform=iOS Simulator,id=$DEV" -derivedDataPath "$DD" \
  build -quiet
xcrun simctl install "$DEV" "$DD/Build/Products/Debug-iphonesimulator/FIZZ.app"
xcrun simctl launch --terminate-running-process "$DEV" com.quixotic7.fizz -demo
echo "launched on $DEV (screenshot: xcrun simctl io $DEV screenshot out.png)"
