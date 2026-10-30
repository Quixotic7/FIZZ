// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// Mock-up 57, the model: a RemoteFM1 unit as THIS iPHONE's panel, as the screens show it (the views: PanelUICard.swift).
//   - The picker's cyan rows (panelRows): USE AS PANEL; in another host's session; over Bluetooth (USB only for the
//     panel); and the attached one, PANEL with ×.
//   - A RemoteFM1 unit plugged in raises no "not FIZZ" notice: the picker opens by itself once with its row
//     (offerPanels, from UnitModel.offline).
//   - USE AS PANEL (usePanelTapped): a backup playing is put away first (the panel is THIS iPHONE's, as a pick of
//     THIS iPHONE puts it away), PLAY's sound stays on the phone, then usePanel.
//   - What the panel does is said in a toast on every tab (watchPanel): linked once, and each release that the user
//     did not ask for (another host took the unit, it restarted, its cable came out, it stopped answering).
//   - ▶ a backup with the panel attached lets the panel go. PanelLink is bound to THIS iPHONE's FizzEmulator: while a
//     backup plays its input would reach the backup's FIZZ (the core is the process's one) and the unit would keep
//     THIS iPHONE's last screen. Feeding the backup's LCD and LEDs instead would need EmulatorHost to claim the unit
//     again on the backup's FIZZ at every ▶ and × (a new link and port each time, the clock rule for a backup), for a
//     mode that is a quick look at an old backup; so the panel lets go, says why, and USE AS PANEL puts the backup away.

import Foundation
import FizzEngine
import FizzKit
import Observation

/// One RemoteFM1 row of the picker.
public struct PanelRow: Sendable, Equatable, Identifiable {
    public enum State: Sendable, Equatable {
        case offer          // USE AS PANEL
        case busy           // another host has a session on it (USE AS PANEL takes it)
        case bluetooth      // heard over Bluetooth: the panel is USB only (RemoteFM1's screen needs it)
        case linking        // claimed, the session not up yet
        case attached       // THIS iPHONE's panel: PANEL with ×
    }
    public let name: String
    public var id: String { name }
    public let via: UnitRow.Via
    /// RemoteFM1's version ("0.4.2")
    public let version: String?
    public let state: State
}

/// The panel and the backup as last seen (their changes make the toasts).
struct PanelSeen: Equatable {
    var state: PanelLink.State?
    var backup: UUID?
}

/// What the panel did, said a moment.
public struct PanelToast: Sendable, Equatable, Identifiable {
    public let id = UUID()
    public let text: String
    /// linked (the green edge); else a quiet one
    public let ok: Bool

    public static let linked = PanelToast(text: "FM-1 linked: it is this iPhone's panel", ok: true)
    public static let backup = PanelToast(text: "FM-1 panel let go: a backup plays", ok: false)
    public static let gone = PanelToast(text: "That FM-1 is not there any more", ok: false)

    /// A release the user did not ask for (nil: let go by hand).
    public static func released(_ r: PanelLink.Release) -> PanelToast? {
        let t: String? = switch r {
        case .byUser: nil
        case .sessionEnded: "FM-1 panel let go: another host took it"
        case .unitReset: "FM-1 panel let go: the FM-1 restarted"
        case .lost: "FM-1 panel let go: the FM-1 is gone"
        case .heartbeat: "FM-1 panel let go: the FM-1 stopped answering"
        case .notFound: "That FM-1 did not answer as RemoteFM1"
        case .refused: "That FM-1 did not take the panel"
        }
        return t.map { PanelToast(text: $0, ok: false) }
    }

    public init(text: String, ok: Bool) { self.text = text; self.ok = ok }
    public static func == (a: Self, b: Self) -> Bool { a.id == b.id }
}

extension PanelUnit {
    /// RemoteFM1's version: HELLO's, else the update identity's ("FM-1_52000402" -> "0.4.2"; a development build's
    /// "FM-1_5200" has none).
    public var version: String? {
        if let v = hello?.version, v.first?.isNumber == true { return v }
        guard let id = identity, id.hasPrefix("FM-1_52") else { return hello?.version }
        let d = Array(id.dropFirst(7))
        guard d.count == 6, d.allSatisfy(\.isNumber) else { return hello?.version }
        return stride(from: 0, to: 6, by: 2).map { String(Int(String(d[$0...$0 + 1])) ?? 0) }.joined(separator: ".")
    }
}

extension UnitModel {
    // MARK: the picker's rows

    /// The RemoteFM1 units as the picker shows them (none without an emulated FIZZ to be the panel of); the attached one
    /// first.
    public var panelRows: [PanelRow] {
        guard let h = emulator else { return [] }
        var units = panelUnits
        if let p = h.panelPort, !units.contains(where: { $0.name == p }) { units.append(PanelUnit(name: p)) }
        return units.map { u in
            let state: PanelRow.State
            if u.name == h.panelPort {
                state = h.panel == .active ? .attached : .linking
            } else if u.via == .bluetooth {
                state = .bluetooth
            } else {
                state = u.busy ? .busy : .offer
            }
            return PanelRow(name: u.name, via: u.via, version: u.version, state: state)
        }
        .sorted { ($0.state == .attached || $0.state == .linking) && !($1.state == .attached || $1.state == .linking) }
    }

    /// A RemoteFM1 unit is THIS iPHONE's panel (claimed or linked).
    public var panelAttached: Bool { emulator?.panelPort != nil }

    // MARK: USE AS PANEL

    /// USE AS PANEL: a backup playing put away first, PLAY's sound on the phone, the unit claimed. false: it is gone.
    @discardableResult
    public func usePanelTapped(_ name: String) async -> Bool {
        guard let h = emulator else { return false }
        if h.backup != nil { await h.endBackup() }
        h.play.sound = .here
        guard usePanel(name) else {
            showPanelToast(.gone)
            return false
        }
        return true
    }

    // MARK: the offer (in the "not FIZZ" notice's place)

    /// The scan's RemoteFM1 units taken out of the probes (their links closed): no notice for them. One newly plugged
    /// in opens the picker once, with its row, while no FM-1 running FIZZ is found beside it and no panel is attached.
    func offerPanels(_ probes: [FizzProbe]) async -> [FizzProbe] {
        let rfm = probes.filter { RemoteFM1Unit.isRemoteFM1(identity: $0.identity.text) }
        let names = Set(rfm.map(\.candidate.name))
        panelOffered.formIntersection(Set(source.scanner.candidates().map(\.name)))     // (gone: offered again)
        guard !rfm.isEmpty else { return probes }
        for p in rfm { await p.link.close() }
        let rest = probes.filter { !names.contains($0.candidate.name) }
        let fresh = names.subtracting(panelOffered)
        panelOffered.formUnion(fresh)
        if !fresh.isEmpty, emulator != nil, !rest.contains(where: { $0.identity.isFizz }) {
            say("RemoteFM1 on \(fresh.sorted().joined(separator: ", ")): offered as \(EmulatorHost.hereName)'s panel")
            // (once THIS iPHONE is the unit: the picker opens on it ticked)
            Task { [weak self] in
                for _ in 0..<40 where self?.onEmulator == false { try? await Task.sleep(for: .milliseconds(50)) }
                guard let self, !self.panelAttached, !self.pickerOpen else { return }
                self.openPicker()
            }
        }
        return rest
    }

    // MARK: the toasts, the backup

    /// The panel's changes watched (once, from start()): the toasts; a backup played lets the panel go.
    func watchPanel() {
        guard let h = emulator else { return }
        withObservationTracking {
            _ = h.panel
            _ = h.backup
        } onChange: { [weak self] in
            Task { @MainActor [weak self] in
                self?.panelMoved()
                self?.watchPanel()
            }
        }
    }

    func panelMoved() {
        guard let h = emulator else { return }
        let now = PanelSeen(state: h.panel, backup: h.backup?.id), was = panelWas
        panelWas = now
        // (▶ a backup with the panel attached: let go, see the top)
        if let b = now.backup, b != was.backup, h.panelPort != nil {
            say("a backup plays: the panel lets go")
            stopPanel()
            showPanelToast(.backup)
            return
        }
        guard now.state != was.state else { return }
        switch now.state {
        case .active:
            say("\(h.panelPort ?? "FM-1"): \(EmulatorHost.hereName)'s panel")
            showPanelToast(.linked)
        case .released(let r):
            say("the panel let go: \(r.rawValue)")
            // (let go for the backup: its toast stays)
            if let t = PanelToast.released(r), now.backup == nil { showPanelToast(t) }
        default:
            break
        }
    }

    /// A toast for a while (a newer one replaces it).
    func showPanelToast(_ t: PanelToast) {
        panelToast = t
        let id = t.id
        Task { [weak self] in
            try? await Task.sleep(for: .seconds(t.ok ? 3.2 : 4))
            if self?.panelToast?.id == id { self?.panelToast = nil }
        }
    }

    // MARK: the demo

    /// `-demo panel -use-panel`: the RemoteFM1 unit taken as the panel once the picker has offered it, the picker put
    /// away first (simulator screenshots); with `-panel-picker` the picker opened again once the toast is gone, with
    /// `-panel-sheet` PLAY's sheet.
    func demoPanel(_ args: [String] = ProcessInfo.processInfo.arguments) {
        guard source is DemoUnitsSource, args.contains("-use-panel") else { return }
        Task {
            for _ in 0..<300 where !(onEmulator && pickerOpen && panelUnits.contains { $0.name == DemoUnitsSource.panelName }) {
                try? await Task.sleep(for: .milliseconds(50))
            }
            try? await Task.sleep(for: .milliseconds(600))
            pickerOpen = false
            try? await Task.sleep(for: .milliseconds(700))         // (the sheet gone: the toast shows over PLAY)
            await usePanelTapped(DemoUnitsSource.panelName)
            guard args.contains("-panel-picker") || args.contains("-panel-sheet") else { return }
            try? await Task.sleep(for: .seconds(4.5))
            if args.contains("-panel-picker") { openPicker(fromPlay: true) } else { emulator?.play.sheet = true }
        }
    }
}
