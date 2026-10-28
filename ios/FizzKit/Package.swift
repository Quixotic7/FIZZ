// swift-tools-version: 6.0
// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
import PackageDescription

let package = Package(
    name: "FizzKit",
    platforms: [.iOS(.v17), .macOS(.v14)],
    products: [
        .library(name: "FizzKit", targets: ["FizzKit"]),
        .library(name: "FizzUI", targets: ["FizzUI"]),
        .library(name: "FizzData", targets: ["FizzData"]),
        .executable(name: "fizzshots", targets: ["fizzshots"]),
    ],
    targets: [
        .target(name: "FizzKit"),
        // the app's screens and models (they also build on macOS, for the snapshots and the tests)
        .target(
            name: "FizzUI",
            dependencies: ["FizzKit", "FizzData"],
            resources: [.copy("Resources/InterTight.ttf"), .copy("Resources/OFL.txt")]
        ),
        // the web Manager's data modules (site/, bundled by ios/jsbundle/build.mjs) run in JavaScriptCore
        .target(
            name: "FizzData",
            dependencies: ["FizzKit"],
            resources: [.copy("Resources/FizzData.js")]
        ),
        // PNGs of the screens with the demo unit's data (macOS, no window)
        .executableTarget(name: "fizzshots", dependencies: ["FizzUI", "FizzKit", "FizzData"]),
        .testTarget(
            name: "FizzKitTests",
            dependencies: ["FizzKit"],
            resources: [.copy("Resources/vectors.json")]
        ),
        .testTarget(
            name: "FizzUITests",
            dependencies: ["FizzUI", "FizzKit", "FizzData"],
            resources: [.copy("Resources/lib.fizzlib")]
        ),
        // (fixtures: node ios/jsbundle/gen_fixtures.mjs; each file copied to the bundle's root, no Resources folder)
        .testTarget(
            name: "FizzDataTests",
            dependencies: ["FizzData"],
            resources: [
                .copy("Resources/fixtures.json"), .copy("Resources/presets.fizzbak"),
                .copy("Resources/lib.fizzlib"), .copy("Resources/deflated.fizzlib"),
            ]
        ),
    ],
    swiftLanguageModes: [.v6]
)
