// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// EDIT's pages (editor.js opsPanel .. playPanel, the VOICE page; mock-ups 11, 17): every one of FIZZ's 115 values on
// a page, each a part of a drawing or a chip in its knob's colour. A MIDI sound: its 28 slots, seven pages of four
// bars (the device's MIDI editor).

import SwiftUI
import FizzData

/// The values' ids (fizz2dn.js Q; the protocol's PARAM ids).
enum QV {
    static let algo = 0, rc = 1, ra = 2, rb1 = 3, harm = 4, dtun = 5, fdbk = 6, mix = 7, ofsc = 8
    static let alev = 15, blev = 19, atrig = 21, arst = 22, phrt = 23, btrig = 25, brst = 26, vel = 27, akey = 28, b1key = 29
    static let fatk = 30, freq = 34, reso = 35, ftype = 36, fdepth = 37, fdly = 38, fktrk = 39, base = 40, width = 41
    static let eatk = 42, edec = 43, esus = 44, erel = 45, level = 46, rb2 = 71, b2key = 72, drive = 73, amod = 74
    static let port = 76, ptim = 77, pslope = 78, drpre = 79, fshape = 80, fgain = 81, bwpost = 82, bits = 83, srr = 84
    static let crpost = 85, voice = 110
    static func lfo(_ k: Int, _ f: Int) -> Int { 47 + 8 * k + f }      // f: 0 SPEED 1 MULT 2 FADE 3 DEST 4 WAVE 5 PHASE 6 TRIG 7 DEPTH
    static func mod(_ k: Int, _ f: Int) -> Int { 86 + 3 * k + f }      // f: 0 SOURCE 1 DEST 2 DEPTH
    static let ratios = [rc, ra, rb1, rb2]
}

/// The pages' icons (editor.js SECTIONS), 26 x 24.
enum PageIcons {
    static func art(_ p: EditPage) -> VectorArt {
        switch p {
        case .ops: VectorArt(26, 24, [.circle(9, 18, 5), .circle(19, 7, 4), .path("M12 14l5-4")])
        case .openv: VectorArt(26, 24, [.path("M2 20 L7 4 L13 12 L22 12 L22 20")])
        case .play: VectorArt(26, 24, [.path("M6 4 L20 12 L6 20 Z")])
        case .filter: VectorArt(26, 24, [.path("M2 8 H12 Q16 8 18 4 Q19 14 24 20")])
        case .amp: VectorArt(26, 24, [.path("M2 20 L5 5 L10 10 L18 10 L23 20")])
        case .drive: VectorArt(26, 24, [.path("M2 12 H6 L9 4 L12 20 L15 4 L18 20 L20 12 H24")])
        case .lfo: VectorArt(26, 24, [.path("M2 12 Q6 2 10 12 Q14 22 18 12 Q22 2 26 12")])
        case .mod: VectorArt(26, 24, [.path("M4 6 h6 M4 12 h6 M4 18 h6 M14 6 L22 18 M14 18 L22 6")])
        case .voice: VectorArt(26, 24, [.circle(8, 12, 4), .circle(18, 12, 4), .path("M2 21 H24")])
        }
    }
    /// the LFO waves (editor.js WAVE_D), 28 x 18
    static let waves = ["M2 14 L8 4 L14 14 L20 4 L26 14", "M2 9 Q5 0 8 9 Q11 18 14 9 Q17 0 20 9 Q23 18 26 9", "M2 14 V4 H9 V14 H16 V4 H23 V14",
                        "M2 4 L12 14 V4 L22 14 V4", "M2 14 L12 4 V14 L22 4 V14", "M2 4 Q4 13 12 14 M12 4 Q14 13 22 14",
                        "M2 10 H7 V4 H12 V13 H17 V7 H22 V11 H26"].map { VectorArt(28, 18, [.path($0)]) }
}

/// One page's values.
struct EditPageView: View {
    let e: EditorModel
    let page: EditPage
    var wide = false

    var body: some View {
        switch page {
        case .ops: OperatorsPage(e: e, wide: wide)
        case .openv: OpEnvPage(e: e, wide: wide)
        case .play: PlayPage(e: e, wide: wide)
        case .filter: FilterPage(e: e, wide: wide)
        case .amp: AmpPage(e: e, wide: wide)
        case .drive: DrivePage(e: e, wide: wide)
        case .lfo: LFOPage(e: e, wide: wide)
        case .mod: ModPage(e: e, wide: wide)
        case .voice: VoicePage(e: e, wide: wide)
        }
    }
}

// MARK: - OPERATORS

/// An operator's card (fizz-ios.html .opc): its RATIO big (dragged up / down), its OFS (dragged sideways), the ratio's
/// bar; lit while turned.
struct OpCard: View {
    let e: EditorModel
    let o: Int
    var big = false

    var body: some View {
        let rid = QV.ratios[o], oid = QV.ofsc + o, c = FZ.operators[o]
        let hot = e.turning.map { $0.id == rid || $0.id == oid } ?? false
        let (w, f) = splitValue(e.text(rid))
        VStack(alignment: .leading, spacing: big ? 10 : 4) {
            HStack {
                Text(AlgoDrawing.names[o]).fz(13, 800, em: 0.12).foregroundStyle(c.color)
                Spacer(minLength: 4)
                Text("OFS \(e.text(oid))").fz(12, 800, em: 0.06).foregroundStyle(FZ.muted.color).monospacedDigit()
            }
            HStack(alignment: .firstTextBaseline, spacing: 0) {
                Text(w).fz(big ? 44 : 29, 800)
                Text(f).fz(big ? 22 : 17, 700).foregroundStyle(FZ.muted.color)
            }
            .foregroundStyle(FZ.text.color).monospacedDigit().lineLimit(1)
            GeometryReader { g in
                ZStack(alignment: .leading) {
                    Capsule().fill(RGB(0x1d1d23).color)
                    Capsule().fill(c.color).frame(width: max(8, g.size.width * (e.table?.frac(rid, e.value(rid)) ?? 0)))
                }
            }
            .frame(height: big ? 12 : 8)
        }
        .padding(.horizontal, big ? 16 : 12).padding(.top, big ? 14 : 7).padding(.bottom, big ? 16 : 9)
        .background(RoundedRectangle(cornerRadius: 16).fill(hot ? c.mix(12, FZ.panel).color : FZ.panel.color))
        .overlay(RoundedRectangle(cornerRadius: 16).strokeBorder(c.color, lineWidth: 2))
        .background(
            RoundedRectangle(cornerRadius: 16).stroke(.black, lineWidth: 3).padding(-3)
                .overlay(RoundedRectangle(cornerRadius: 19).stroke(c.color, lineWidth: 2).padding(-5))
                .opacity(hot ? 1 : 0)
        )
        .shadow(color: hot || (big && e.op == o) ? c.opacity(0.5) : .clear, radius: hot ? 17 : 11)
        .sensoryFeedback(.selection, trigger: e.value(rid))
        .sensoryFeedback(.selection, trigger: e.value(oid))
        .turns(e, y: rid, x: oid, label: "\(AlgoDrawing.names[o]) RATIO", tint: c, xSpan: 260, tap: { e.op = o })
        .turnDim(e, [rid, oid])
    }
}

/// ALGO: a big pink number; dragged up / down, a tap the next one.
struct AlgoNumber: View {
    let e: EditorModel
    var size: CGFloat = 104
    var body: some View {
        VStack(alignment: .leading, spacing: -size * 0.12) {
            Text("ALGO").fz(size * 0.13, 800, em: 0.08).foregroundStyle(FZ.kalg.color)
            Text("\(e.value(QV.algo))").fz(size, 800).foregroundStyle(FZ.kalg.color).monospacedDigit()
        }
        .fixedSize()
        .sensoryFeedback(.selection, trigger: e.value(QV.algo))
        .turns(e, y: QV.algo, label: "ALGO", tint: FZ.kalg, ySpan: 240, tap: { e.set(QV.algo, e.value(QV.algo) % 8 + 1) })
        .turnDim(e, [QV.algo])
    }
}

struct OperatorDials: View {
    let e: EditorModel
    var size: CGFloat = 52
    var body: some View {
        HStack(spacing: 0) {
            EditDial(e: e, id: QV.harm, label: "HARM", tint: FZ.k1, size: size, valueSize: size > 60 ? 22 : 18)
            Spacer(minLength: 0)
            EditDial(e: e, id: QV.dtun, label: "DTUN", tint: FZ.k2, size: size, valueSize: size > 60 ? 22 : 18)
            Spacer(minLength: 0)
            EditDial(e: e, id: QV.fdbk, label: "FDBK", tint: FZ.k4, size: size, valueSize: size > 60 ? 22 : 18)
            Spacer(minLength: 0)
            EditDial(e: e, id: QV.mix, label: "MIX", tint: FZ.k3, size: size, valueSize: size > 60 ? 22 : 18)
            Spacer(minLength: 0)
            EditDial(e: e, id: QV.drive, label: "DRIVE", tint: FZ.orange, size: size, valueSize: size > 60 ? 22 : 18)
        }
    }
}

struct OperatorsPage: View {
    let e: EditorModel
    var wide = false
    var body: some View {
        if wide {
            VStack(spacing: 0) {
                HStack(alignment: .center, spacing: 16) {
                    ZStack(alignment: .topLeading) {
                        AlgoDrawing(e: e, crop: CGRect(x: 110, y: 40, width: 490, height: 340))
                        AlgoNumber(e: e, size: 120).padding(.top, 30)
                    }
                    .frame(maxWidth: .infinity)
                    Grid(horizontalSpacing: 12, verticalSpacing: 12) {
                        GridRow { OpCard(e: e, o: 0, big: true); OpCard(e: e, o: 1, big: true) }
                        GridRow { OpCard(e: e, o: 2, big: true); OpCard(e: e, o: 3, big: true) }
                    }
                    .frame(width: 300)
                }
                .frame(maxHeight: .infinity)
                Rectangle().fill(FZ.line.color).frame(height: 1).padding(.bottom, 12)
                OperatorDials(e: e, size: 66).padding(.horizontal, 60)
            }
        } else {
            VStack(spacing: 8) {
                ZStack(alignment: .topLeading) {
                    AlgoDrawing(e: e).frame(height: 200)
                    AlgoNumber(e: e)
                }
                Grid(horizontalSpacing: 8, verticalSpacing: 8) {
                    GridRow { OpCard(e: e, o: 0); OpCard(e: e, o: 1) }
                    GridRow { OpCard(e: e, o: 2); OpCard(e: e, o: 3) }
                }
                OperatorDials(e: e).padding(.top, 6)
            }
        }
    }
}

// MARK: - OP ENV

struct OpEnvPage: View {
    let e: EditorModel
    var wide = false
    var body: some View {
        let cards = ForEach(0..<2, id: \.self) { k in
            let ids = k == 1 ? (atk: 16, dec: 17, end: 18, lev: QV.blev, dly: 24, trg: QV.btrig, rst: QV.brst)
                : (atk: 12, dec: 13, end: 14, lev: QV.alev, dly: 20, trg: QV.atrig, rst: QV.arst)
            let c = k == 1 ? FZ.k3 : FZ.k2
            EditCard(title: k == 1 ? "ENV B" : "ENV A", tint: c, trailing: AnyView(HStack(spacing: 6) {
                Text(k == 1 ? "B1 · B2" : "A").fz(12, 800, em: 0.08).foregroundStyle(FZ.muted.color)
                EditToggle(e: e, id: ids.trg, label: "TRIG", tint: c)
                EditToggle(e: e, id: ids.rst, label: "RESET", tint: c)
            })) {
                OpEnvDrawing(e: e, k: k)
                HStack(spacing: 6) {
                    EditChip(e: e, id: ids.atk, label: "ATK", tint: c)
                    EditChip(e: e, id: ids.dec, label: "DEC", tint: c)
                    EditChip(e: e, id: ids.end, label: "END", tint: c)
                    EditChip(e: e, id: ids.dly, label: "DLY", tint: c)
                }
                EditBar(e: e, id: ids.lev, label: "LEVEL", tint: c)
            }
        }
        if wide { HStack(alignment: .top, spacing: 14) { cards } } else { VStack(spacing: 12) { cards } }
    }
}

// MARK: - PLAY

struct PlayPage: View {
    let e: EditorModel
    var wide = false
    var body: some View {
        VStack(alignment: .leading, spacing: 14) {
            Caption(label: "PHASE RESET") { EditSeg(e: e, id: QV.phrt, tint: FZ.k1) }
            VStack(spacing: 6) {
                EditBar(e: e, id: QV.akey, label: "A KEY", tint: FZ.k2)
                EditBar(e: e, id: QV.b1key, label: "B1 KEY", tint: FZ.k3)
                EditBar(e: e, id: QV.b2key, label: "B2 KEY", tint: FZ.k4)
                EditBar(e: e, id: QV.vel, label: "VEL", tint: FZ.k1)
            }
            EditCard(title: "PORTAMENTO", tint: FZ.yellow) {
                GlideDrawing(e: e)
                EditSeg(e: e, id: QV.port, tint: FZ.yellow)
                HStack(spacing: 10) {
                    EditChip(e: e, id: QV.ptim, label: "TIME", tint: FZ.yellow)
                    Caption(label: "SLOPE") { EditSeg(e: e, id: QV.pslope, tint: FZ.yellow, height: 32) }
                }
            }
        }
    }
}

// MARK: - FILTER

struct FilterPage: View {
    let e: EditorModel
    var wide = false
    static let machines: [(String, [Int])] = [("LEGACY", [0, 1, 2]), ("LP4", [3]), ("MULTI", [4]), ("COMB+", [5]), ("COMB−", [6]), ("EQ", [7])]

    var body: some View {
        let ft = e.value(QV.ftype), m = Self.machines.firstIndex { $0.1.contains(ft) } ?? 0
        VStack(alignment: .leading, spacing: 12) {
            machineSeg(ft: ft, m: m)
            if m == 0 { EditSeg(e: e, id: QV.ftype, tint: FZ.k4, pairs: [(0, "OFF"), (1, "LP12"), (2, "HP12")], height: 32) }
            let curve = VStack(alignment: .leading, spacing: 8) {
                FilterCurve(e: e)
                HStack(spacing: 6) {
                    if m == 5 { EditChip(e: e, id: QV.fgain, label: "GAIN", tint: FZ.k2) } else {
                        EditChip(e: e, id: QV.reso, label: (3...4).contains(m) ? "FDBK" : "RESO", tint: FZ.k2)
                    }
                    if m >= 2 { EditChip(e: e, id: QV.fshape, label: m == 2 ? "TYPE" : m == 5 ? "Q" : "LPF", tint: FZ.k3) }
                    EditChip(e: e, id: QV.fktrk, label: "KEY TRK", tint: FZ.k3)
                }
            }
            let env = EditCard(title: "ENV", tint: FZ.k4) {
                ADSRDrawing(e: e, ids: [30, 31, 32, 33], tint: FZ.k4, w: 420, h: 130, name: "FILTER")
                HStack(spacing: 6) {
                    EditChip(e: e, id: QV.fdepth, label: "DEPTH", tint: FZ.k4, big: true)
                    EditChip(e: e, id: QV.fdly, label: "DLY", tint: FZ.k4)
                }
                HStack(spacing: 6) {
                    EditChip(e: e, id: 30, label: "ATK", tint: FZ.k4)
                    EditChip(e: e, id: 31, label: "DEC", tint: FZ.k4)
                    EditChip(e: e, id: 32, label: "SUS", tint: FZ.k4)
                    EditChip(e: e, id: 33, label: "REL", tint: FZ.k4)
                }
            }
            if wide { HStack(alignment: .top, spacing: 14) { curve; env } } else { curve; env }
            HStack(spacing: 6) {
                EditChip(e: e, id: QV.base, label: "BASE", tint: FZ.k1)
                EditChip(e: e, id: QV.width, label: "WIDTH", tint: FZ.k2)
                EditSeg(e: e, id: QV.bwpost, tint: FZ.k3, pairs: [(0, "BEFORE"), (1, "AFTER")], height: 32)
            }
        }
    }

    /// The machine (LEGACY keeps OFF / LP12 / HP12).
    func machineSeg(ft: Int, m: Int) -> some View {
        HStack(spacing: 0) {
            ForEach(Self.machines.indices, id: \.self) { k in
                let on = k == m
                Button { e.set(QV.ftype, k == 0 ? (ft <= 2 ? ft : 1) : Self.machines[k].1[0]) } label: {
                    Text(Self.machines[k].0).fz(12, 800, em: 0.04).lineLimit(1).minimumScaleFactor(0.7)
                        .foregroundStyle(on ? Color.black : FZ.muted.color)
                        .frame(maxWidth: .infinity).frame(height: 36)
                        .background(on ? FZ.k4.color : .clear)
                        .contentShape(Rectangle())
                }
                .buttonStyle(.plain)
            }
        }
        .clipShape(Capsule())
        .overlay(Capsule().strokeBorder(FZ.line.color, lineWidth: 1.5))
        .sensoryFeedback(.selection, trigger: ft)
        .turnDim(e, [QV.ftype])
    }
}

// MARK: - AMP, DRIVE · CRUSH

struct AmpPage: View {
    let e: EditorModel
    var wide = false
    var body: some View {
        let ahd = e.value(QV.amod) == 1
        VStack(alignment: .leading, spacing: 12) {
            EditSeg(e: e, id: QV.amod, tint: FZ.k3).frame(width: 200)
            ADSRDrawing(e: e, ids: [QV.eatk, QV.edec, QV.esus, QV.erel], tint: FZ.k3, w: 520, h: 170, ahd: ahd, name: "AMP")
            HStack(spacing: 6) {
                EditChip(e: e, id: QV.eatk, label: "ATK", tint: FZ.k3)
                if ahd {
                    EditChip(e: e, id: 75, label: "HOLD", tint: FZ.yellow)
                    EditChip(e: e, id: QV.edec, label: "DEC", tint: FZ.k3)
                } else {
                    EditChip(e: e, id: QV.edec, label: "DEC", tint: FZ.k3)
                    EditChip(e: e, id: QV.esus, label: "SUS", tint: FZ.k3)
                    EditChip(e: e, id: QV.erel, label: "REL", tint: FZ.k3)
                }
            }
            EditBar(e: e, id: QV.level, label: "LEVEL", tint: FZ.k3)
        }
    }
}

struct DrivePage: View {
    let e: EditorModel
    var wide = false
    var body: some View {
        VStack(alignment: .leading, spacing: 14) {
            HStack(spacing: 18) {
                EditDial(e: e, id: QV.drive, label: "DRIVE", tint: FZ.orange, size: 96, valueSize: 30)
                EditSeg(e: e, id: QV.drpre, tint: FZ.orange, pairs: [(0, "AFTER FILTER"), (1, "BEFORE")])
            }
            EditCard(title: "CRUSH", tint: FZ.pink) {
                CrushDrawing(e: e)
                HStack(spacing: 6) {
                    EditChip(e: e, id: QV.bits, label: "BITS", tint: FZ.pink)
                    EditChip(e: e, id: QV.srr, label: "SRR", tint: FZ.pink)
                }
                EditSeg(e: e, id: QV.crpost, tint: FZ.pink, pairs: [(0, "BEFORE FILTER"), (1, "AFTER")])
            }
        }
    }
}

// MARK: - LFO

struct LFOPage: View {
    let e: EditorModel
    var wide = false
    static let tints = [FZ.cyan, FZ.yellow, FZ.violet]

    var body: some View {
        let k = e.lfo, c = Self.tints[k]
        VStack(alignment: .leading, spacing: 12) {
            HStack(spacing: 8) {
                ForEach(0..<3, id: \.self) { i in
                    let on = i == k
                    Button { e.lfo = i } label: {
                        Text("LFO \(i + 1)").fz(14, 800, em: 0.08)
                            .foregroundStyle(on ? Color.black : FZ.text.color)
                            .frame(maxWidth: .infinity).frame(height: 38)
                            .background(Capsule().fill(on ? Self.tints[i].color : Self.tints[i].opacity(0.08)))
                            .overlay(Capsule().strokeBorder(Self.tints[i].mix(on ? 100 : 50, FZ.line).color, lineWidth: 1.5))
                    }
                    .buttonStyle(.plain)
                }
            }
            EditCard(title: "LFO \(k + 1)", tint: c, trailing: AnyView(EditChip(e: e, id: QV.lfo(k, 3), label: "DEST", tint: c, pick: true))) {
                LFOWave(e: e, k: k, tint: c)
                LazyVGrid(columns: [GridItem(.adaptive(minimum: 98), spacing: 6)], alignment: .leading, spacing: 6) {
                    EditChip(e: e, id: QV.lfo(k, 0), label: "SPEED", tint: c)
                    EditChip(e: e, id: QV.lfo(k, 7), label: "DEPTH", tint: c)
                    EditChip(e: e, id: QV.lfo(k, 1), label: "MULT", tint: c)
                    EditChip(e: e, id: QV.lfo(k, 2), label: "FADE", tint: c)
                    EditChip(e: e, id: QV.lfo(k, 5), label: "PHASE", tint: c)
                }
                waveSeg(QV.lfo(k, 4), c)
                EditSeg(e: e, id: QV.lfo(k, 6), tint: c, height: 32)
            }
        }
    }

    func waveSeg(_ id: Int, _ c: RGB) -> some View {
        HStack(spacing: 0) {
            ForEach(0..<7, id: \.self) { w in
                let on = e.value(id) == w
                Button { e.set(id, w) } label: {
                    IconView(PageIcons.waves[w], color: on ? .black : c.color, lineWidth: 2.2, backdrop: on ? c.color : .black)
                        .frame(width: 26, height: 17)
                        .frame(maxWidth: .infinity).frame(height: 36)
                        .background(on ? c.color : .clear)
                        .contentShape(Rectangle())
                }
                .buttonStyle(.plain)
            }
        }
        .clipShape(Capsule())
        .overlay(Capsule().strokeBorder(FZ.line.color, lineWidth: 1.5))
        .sensoryFeedback(.selection, trigger: e.value(id))
        .turnDim(e, [id])
    }
}

// MARK: - MOD

struct ModPage: View {
    let e: EditorModel
    var wide = false
    var body: some View {
        VStack(spacing: 7) {
            ForEach(0..<8, id: \.self) { k in row(k) }
        }
    }

    @ViewBuilder func row(_ k: Int) -> some View {
        let src = e.value(QV.mod(k, 0)), dp = e.value(QV.mod(k, 2)), c = dp < 0 ? FZ.orange : FZ.cyan
        HStack(spacing: 6) {
            Text("\(k + 1)").fz(14, 800).foregroundStyle(FZ.muted.color).frame(width: 16)
            if src == 0 {
                Button { e.set(QV.mod(k, 0), 1) } label: {
                    Text("+").fz(20, 800).foregroundStyle(FZ.kalg.color)
                        .frame(maxWidth: .infinity).frame(height: 44)
                        .background(RoundedRectangle(cornerRadius: 12).strokeBorder(FZ.kalg.mix(35, FZ.line).color, style: StrokeStyle(lineWidth: 1.5, dash: [5, 4])))
                }
                .buttonStyle(.plain)
                .turnDim(e, [])
            } else {
                EditChip(e: e, id: QV.mod(k, 0), label: "SRC", tint: FZ.kalg, pick: true)
                ModCable(e: e, id: QV.mod(k, 2), tint: c).frame(maxWidth: wide ? 200 : 60)
                EditChip(e: e, id: QV.mod(k, 1), label: "DEST", tint: FZ.text, pick: true)
                EditChip(e: e, id: QV.mod(k, 2), label: "DEPTH", tint: c)
                Button { e.set(QV.mod(k, 0), 0) } label: {
                    Image(systemName: "xmark").font(.system(size: 13, weight: .heavy)).foregroundStyle(FZ.muted.color)
                        .frame(width: 30, height: 44).contentShape(Rectangle())
                }
                .buttonStyle(.plain)
                .turnDim(e, [])
            }
        }
    }
}

// MARK: - VOICE

struct VoicePage: View {
    let e: EditorModel
    var wide = false
    var body: some View {
        VStack(alignment: .leading, spacing: 16) {
            EditSeg(e: e, id: QV.voice, tint: FZ.k1).frame(width: 220)
            HStack(spacing: 0) {
                EditDial(e: e, id: QV.voice + 1, label: "UNISON", tint: FZ.k2, size: 70, valueSize: 22)
                Spacer(minLength: 0)
                EditDial(e: e, id: QV.voice + 2, label: "DETUNE", tint: FZ.k3, size: 70, valueSize: 22)
                Spacer(minLength: 0)
                EditDial(e: e, id: QV.voice + 3, label: "SPREAD", tint: FZ.k4, size: 70, valueSize: 22)
            }
            .padding(.horizontal, wide ? 80 : 10)
            Caption(label: "PRIORITY") { EditSeg(e: e, id: QV.voice + 4, tint: FZ.kalg) }
        }
    }
}

// MARK: - A MIDI sound

/// A MIDI sound's page n (0..6): four slots in the knobs' colours, each a bar (its value, dragged), its CC number (a
/// tap types it) and value ("—" untouched; × or a double tap makes it untouched).
struct MidiPage: View {
    let e: EditorModel
    var height: CGFloat = 260
    @State private var typing: Int?
    @State private var cc = ""

    var body: some View {
        let n = e.midiPage
        HStack(spacing: 10) {
            ForEach(0..<4, id: \.self) { j in slot(n * 4 + j, FZ.operators[j]) }
        }
        .alert("CC NUMBER", isPresented: Binding(get: { typing != nil }, set: { if !$0 { typing = nil } })) {
            TextField("0 .. 127", text: $cc)
            Button("OK") { if let k = typing, let v = Int(cc.filter(\.isNumber).prefix(3)) { e.setSlot(k, cc: v) }; typing = nil }
            Button("CANCEL", role: .cancel) { typing = nil }
        }
    }

    func slot(_ k: Int, _ c: RGB) -> some View {
        let x = e.sound?.midi.flatMap { $0.indices.contains(k) ? $0[k] : nil } ?? EditMidiSlot(cc: 0, value: 0, touched: false)
        let hot = e.turning?.id == -1 - k
        return VStack(spacing: 8) {
            GeometryReader { g in
                ZStack(alignment: .bottom) {
                    RoundedRectangle(cornerRadius: 14).fill(RGB(0x15151a).color)
                    if x.touched {
                        RoundedRectangle(cornerRadius: 14).fill(c.color)
                            .frame(height: max(14, g.size.height * CGFloat(x.value) / 127))
                    } else {
                        RoundedRectangle(cornerRadius: 14).strokeBorder(c.mix(45, FZ.line).color, style: StrokeStyle(lineWidth: 2, dash: [6, 5]))
                    }
                }
                .overlay(RoundedRectangle(cornerRadius: 14).strokeBorder(hot ? c.color : .clear, lineWidth: 2))
                .shadow(color: hot ? c.opacity(0.5) : .clear, radius: 12)
                .contentShape(Rectangle())
                .onTapGesture(count: 2) { e.untouch(k) }
                .highPriorityGesture(DragGesture(minimumDistance: 0).onChanged { d in
                    if e.turning == nil, !e.locked { e.turning = EditTurn(id: -1 - k, label: "CC \(x.cc)", tint: c) }
                    let v = Int((max(0, min(1, (g.size.height - d.location.y) / max(1, g.size.height))) * 127).rounded())
                    e.setSlot(k, value: v, touched: true)
                }.onEnded { _ in e.turning = nil })
            }
            .frame(height: height)
            Button { cc = "\(x.cc)"; typing = k } label: {
                Text("CC \(x.cc)").fz(13, 800, em: 0.06).foregroundStyle(c.color)
                    .padding(.horizontal, 10).frame(height: 30)
                    .overlay(Capsule().strokeBorder(c.mix(50, FZ.line).color, lineWidth: 1.5))
            }
            .buttonStyle(.plain)
            HStack(spacing: 6) {
                Text(x.touched ? "\(x.value)" : "—").fz(22, 800).monospacedDigit().foregroundStyle(FZ.text.color)
                if x.touched {
                    Button { e.untouch(k) } label: {
                        Image(systemName: "xmark").font(.system(size: 11, weight: .heavy)).foregroundStyle(FZ.muted.color)
                    }
                    .buttonStyle(.plain)
                }
            }
        }
        .frame(maxWidth: .infinity)
        .sensoryFeedback(.selection, trigger: x.value)
        .opacity(e.turning.map { $0.id != -1 - k } ?? false ? 0.13 : 1)
    }
}
