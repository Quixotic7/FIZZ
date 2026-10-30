// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2026 RemoteFM1 contributors
// Derived from RemoteFM1IOS af91bc5 RFMKit/Sources/RFMKit/OMX27/OmxScreen.swift, unchanged but for this line (RFMFonts is FIZZ's own: MidiPanel.swift).
// The OMX-27's 128 x 32 screen, drawn (the website's omx-link.js render): the view's name large, its context small,
// four marks for K1's views; 1-bit at 50%, packed for the OMX. Chakra Petch Bold (bundled), the title from 19 px down
// until it fits 100 px, the line from 11 px until it fits 126 px. CoreGraphics only: the Mac tests draw it too.

import CoreGraphics
import CoreText
import Foundation

public enum OmxScreen {
    /// the screen as 128 x 32 bits (bits[y * 128 + x], 1 lit, y down)
    public static func bits(_ s: OmxViews.Screen) -> [UInt8] {
        RFMFonts.register()
        let W = OmxWire.W, H = OmxWire.H
        var px = [UInt8](repeating: 0, count: W * H)
        px.withUnsafeMutableBytes { buf in
            guard let g = CGContext(data: buf.baseAddress, width: W, height: H, bitsPerComponent: 8, bytesPerRow: W,
                                    space: CGColorSpaceCreateDeviceGray(), bitmapInfo: CGImageAlphaInfo.none.rawValue)
            else { return }
            g.setFillColor(gray: 0, alpha: 1)
            g.fill(CGRect(x: 0, y: 0, width: W, height: H))
            g.setFillColor(gray: 1, alpha: 1)
            // (a canvas rect x, y, w, h, y down)
            func rect(_ x: Int, _ y: Int, _ w: Int, _ h: Int) { g.fill(CGRect(x: x, y: H - y - h, width: w, height: h)) }
            for i in 0..<4 {
                if i == s.view.rawValue { rect(104 + i * 6, 2, 4, 6) } else { rect(104 + i * 6, 7, 4, 1) }
            }
            g.setAllowsAntialiasing(true)
            g.setShouldSmoothFonts(false)
            g.textMatrix = .identity
            text(g, s.title, from: 19, min: 10, fits: 100, x: 1, baseline: H - 17)
            text(g, s.line, from: 11, min: 7, fits: 126, x: 1, baseline: H - 30)
        }
        return px.map { $0 >= 128 ? 1 : 0 }
    }

    /// the screen packed for the OMX (512 bytes)
    public static func render(_ s: OmxViews.Screen) -> [UInt8] { OmxWire.pack(bits(s)) }

    /// (the canvas's loop: a size, then smaller while too wide, the last one tried kept)
    private static func text(_ g: CGContext, _ s: String, from: Int, min: Int, fits: Double, x: Int, baseline: Int) {
        var px = from
        var line: CTLine
        repeat {
            line = make(s, CGFloat(px))
            px -= 1
        } while CTLineGetTypographicBounds(line, nil, nil, nil) > fits && px > min
        g.textPosition = CGPoint(x: x, y: baseline)
        CTLineDraw(line, g)
    }

    private static func make(_ s: String, _ size: CGFloat) -> CTLine {
        let font = CTFontCreateWithName(RFMFonts.hardwareName as CFString, size, nil)
        let white = CGColor(gray: 1, alpha: 1)
        let attrs: [NSAttributedString.Key: Any] = [
            NSAttributedString.Key(kCTFontAttributeName as String): font,
            NSAttributedString.Key(kCTForegroundColorAttributeName as String): white,
        ]
        return CTLineCreateWithAttributedString(NSAttributedString(string: s, attributes: attrs))
    }
}
