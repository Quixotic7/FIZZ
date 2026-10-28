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
        .executable(name: "fizzshots", targets: ["fizzshots"]),
    ],
    targets: [
        .target(name: "FizzKit"),
        // the app's screens and models (they also build on macOS, for the snapshots and the tests)
        .target(
            name: "FizzUI",
            dependencies: ["FizzKit"],
            resources: [.copy("Resources/InterTight.ttf"), .copy("Resources/OFL.txt")]
        ),
        // PNGs of the screens with the demo unit's data (macOS, no window)
        .executableTarget(name: "fizzshots", dependencies: ["FizzUI", "FizzKit"]),
        .testTarget(
            name: "FizzKitTests",
            dependencies: ["FizzKit"],
            resources: [.copy("Resources/vectors.json")]
        ),
        .testTarget(name: "FizzUITests", dependencies: ["FizzUI", "FizzKit"]),
    ],
    swiftLanguageModes: [.v6]
)
