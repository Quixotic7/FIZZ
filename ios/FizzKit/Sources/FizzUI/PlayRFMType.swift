// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// PLAY's sheet and chip lettering (mock-ups 49-54): DM Sans (the text; its optical size follows the point size, as a
// browser sets it) and Unbounded (the sheet's title), both variable fonts under the SIL OFL 1.1
// (Resources/OFL-DMSans.txt, Resources/OFL-Unbounded.txt), registered for the process from the package's bundle.

import CoreText
import Foundation
import SwiftUI

enum RFMType {
    private static func register(_ file: String) -> Bool {
        guard let url = Bundle.module.url(forResource: file, withExtension: "ttf") else { return false }
        var err: Unmanaged<CFError>?
        if CTFontManagerRegisterFontsForURL(url as CFURL, .process, &err) { return true }
        let e = err?.takeRetainedValue()
        return e.map { CFErrorGetCode($0) == CTFontManagerError.alreadyRegistered.rawValue } ?? false
    }

    private static let hasDMSans: Bool = register("DMSans")
    private static let hasUnbounded: Bool = register("Unbounded")

    private static let wght = 0x7767_6874     // 'wght'
    private static let opsz = 0x6F70_737A     // 'opsz'

    private static func variable(_ name: String, _ size: CGFloat, _ axes: [Int: Double]) -> Font {
        let attrs: [CFString: Any] = [kCTFontNameAttribute: name, kCTFontVariationAttribute: axes]
        let d = CTFontDescriptorCreateWithAttributes(attrs as CFDictionary)
        return Font(CTFontCreateWithFontDescriptor(d, size, nil))
    }

    private static func fallback(_ size: CGFloat, _ weight: Double) -> Font {
        let w: Font.Weight = weight >= 650 ? .bold : weight >= 550 ? .semibold : weight >= 450 ? .medium : .regular
        return Font.system(size: size, weight: w)
    }

    /// DM Sans at `weight` (100..1000), its optical size the point size (9..40).
    static func dm(_ size: CGFloat, _ weight: Double = 500) -> Font {
        hasDMSans ? variable("DMSans-9ptRegular", size, [wght: weight, opsz: Double(max(9, min(40, size)))]) : fallback(size, weight)
    }

    /// Unbounded at `weight` (200..900).
    static func unbounded(_ size: CGFloat, _ weight: Double = 700) -> Font {
        hasUnbounded ? variable("Unbounded-Regular", size, [wght: weight]) : fallback(size, weight)
    }
}

extension View {
    /// DM Sans, `size` pt; `em`: CSS letter-spacing in em.
    func dm(_ size: CGFloat, _ weight: Double = 500, em: CGFloat = 0) -> some View {
        font(RFMType.dm(size, weight)).tracking(size * em)
    }
}
