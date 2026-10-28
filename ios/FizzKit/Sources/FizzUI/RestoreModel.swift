// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// RESTORE and ADD PRESETS from a backup (manager.js openFile / openRestore / rsFit / rsPresets / restoreFull /
// addPresets): a kept backup or an opened file read (fizzbak.js readBackup), then how it goes onto this unit
// (entry.js restorePlan): the whole store when the backup has an image of a layout this unit can take; another
// package's (USB onto Bluetooth, or back) written again in this one's layout with the projects and MY PRESETS chosen
// (projects.js fitModel); otherwise its presets added, the ones already there skipped, as many as MY PRESETS has
// places for. Over Bluetooth a restore is refused up front (FZER 15: the cable). The writes are UnitActions' (the
// safety backup first, the progress screen, the failures).

import Foundation
import FizzData
import FizzKit
import Observation

@MainActor @Observable
public final class RestoreModel {
    /// The backup offered on the sheet.
    public struct Offer: Sendable {
        public var name: String
        public var date: Date
        public var backup: FizzBackupSummary
        public var plan: FizzRestorePlan
        public var full: Bool { plan.mode == .full && backup.image != nil }
    }

    /// The RESTORE / ADD PRESETS sheet.
    public var presented = false
    public private(set) var offer: Offer?
    /// Reading the file (the sheet waits).
    public private(set) var loading = false
    /// A file that did not read, or a choice that does not fit (a short state on the sheet).
    public private(set) var problem: String?
    /// Over Bluetooth: no restore (the cable).
    public private(set) var viaBle = false
    /// Another package's backup: the projects and MY PRESETS kept (0-based), and what is left out of it.
    public private(set) var choice: FizzFitChoice?
    public private(set) var left: FizzRestoreFit?
    /// ADD: the backup's presets not on the unit yet (indices into its presets), those ticked, MY PRESETS' free places.
    public private(set) var fresh: [Int] = []
    public private(set) var picked: Set<Int> = []
    public private(set) var room = 0
    /// ADD running, and what it came to.
    public private(set) var adding = false
    public private(set) var added: AddResult?

    /// The progress screen's subject in cyan ("NOIR" for WRITE TO FM-1: set it before writeProjects) and its
    /// sectors drawn in that colour. nil: none.
    // WIRE AT MERGE: I6 sets subject / highlight before actions.writeProjects (mock-up 20's "WRITING NOIR").
    public var subject: String?
    public var highlight: Range<Int>?
    /// The last restore started here: AGAIN after FZER 10, ADD PRESETS after FZER 11.
    public private(set) var last: (image: [UInt8], name: String)?

    @ObservationIgnored weak var unit: UnitModel?

    init() {}

    // MARK: offering

    /// A kept backup's RESTORE (its row swiped).
    public func present(_ e: BackupEntry) async {
        guard let u = unit else { return }
        guard let d = try? Data(contentsOf: u.backups.store.url(of: e)) else { problem = "NOT FOUND"; presented = true; return }
        await present(bytes: [UInt8](d), name: e.name, date: e.date)
    }

    /// A backup's bytes (a .fizzbak, a .fizzlib, a store .bin): read, its plan made, the sheet shown.
    public func present(bytes: [UInt8], name: String, date: Date) async {
        guard let u = unit else { return }
        reset()
        loading = true
        presented = true
        defer { loading = false }
        do {
            let e = try FizzDataShared.engine()
            let b = try await e.readBackup(bytes, fileName: name)
            let plan = try await e.restorePlan(image: b.image, unitSize: Int(u.storeInfo?.size ?? 0))
            if !plan.error.isEmpty { u.say("restore: \(plan.error)") }
            offer = Offer(name: name, date: b.date ?? date, backup: b, plan: plan)
            viaBle = await u.link?.viaBle ?? false
            if let fit = plan.fit, offer?.full == true {
                choice = fit.choice
                await refit()
            } else if offer?.full != true {
                await pickPresets()
            }
        } catch {
            u.say("open \(name): \(error)")
            problem = "NOT A BACKUP"
        }
    }

    /// A file opened in BACKUP (the picker, Open in FIZZ): kept, then offered when a unit is connected.
    public func open(_ url: URL) async {
        guard let u = unit, await u.backups.open(url) else { return }
        guard u.link != nil, let name = u.backups.added, let e = u.backups.store.entries.first(where: { $0.name == name }) else { return }
        await present(e)
    }

    public func close() {
        guard !adding else { return }
        presented = false
    }

    private func reset() {
        offer = nil; problem = nil; choice = nil; left = nil
        fresh = []; picked = []; room = 0; added = nil
    }

    // MARK: the numbers the sheet shows

    /// FM-1 NOW: its projects and MY PRESETS.
    public var now: (projects: Int, presets: Int) { (unit?.status?.projects ?? 0, unit?.mine.count ?? 0) }

    /// After: the backup's (RESTORE: what was chosen), or now plus those added (ADD).
    public var then: (projects: Int, presets: Int) {
        guard let o = offer else { return now }
        if o.full {
            if let c = choice { return (c.projects.count, c.presets.count) }
            return (o.backup.projects?.count ?? 0, o.backup.presetCount)
        }
        return (now.projects, min(nlib, now.presets + picked.count))
    }

    /// The package's projects and MY PRESETS (STATUS's, else the layout's: manager.js unitPkg).
    public var nlib: Int { unit?.status.flatMap { $0.nlib > 0 ? $0.nlib : nil } ?? offer?.plan.to?.nlib ?? 256 }
    public var nproj: Int { unit?.status.flatMap { $0.nproj > 0 ? $0.nproj : nil } ?? offer?.plan.to?.nproj ?? 8 }

    /// Another package's backup holds more than this one has places for: the choice is shown.
    public var mustChoose: Bool {
        guard let o = offer, let f = o.plan.fit, let to = o.plan.to else { return false }
        return f.projects.count > to.nproj || f.presets.count > to.nlib
    }

    /// The choice fits the package.
    public var fits: Bool {
        guard let c = choice, let to = offer?.plan.to else { return true }
        return c.projects.count <= to.nproj && c.presets.count <= to.nlib
    }

    /// RESTORE can go: a full backup, not over Bluetooth, the choice fits, nothing else running.
    public var canRestore: Bool {
        guard let o = offer, o.full, !viaBle, fits, let a = unit?.actions else { return false }
        return a.canTransfer
    }

    public var canAdd: Bool {
        guard let o = offer, !o.full, !picked.isEmpty, !adding, let a = unit?.actions else { return false }
        return a.canTransfer
    }

    // MARK: choosing

    public func toggleProject(_ p: Int) async {
        guard var c = choice, let to = offer?.plan.to else { return }
        if let k = c.projects.firstIndex(of: p) { c.projects.remove(at: k) } else if c.projects.count < to.nproj { c.projects.append(p) }
        c.projects.sort()
        choice = c
        await refit()
    }

    public func togglePreset(_ i: Int) async {
        guard var c = choice, let to = offer?.plan.to else { return }
        if let k = c.presets.firstIndex(of: i) { c.presets.remove(at: k) } else if c.presets.count < to.nlib { c.presets.append(i) }
        c.presets.sort()
        choice = c
        await refit()
    }

    /// ADD: a preset ticked or not (no more than MY PRESETS' free places).
    public func togglePick(_ i: Int) {
        if picked.contains(i) { picked.remove(i) } else if picked.count < room { picked.insert(i) }
    }

    /// What the choice leaves out (fitModel's left).
    private func refit() async {
        guard let o = offer, let image = o.backup.image, let c = choice, let u = unit else { return }
        problem = nil
        do {
            left = try await FizzDataShared.engine().restoreFit(image: image, unitSize: Int(u.storeInfo?.size ?? 0), choice: c, withImage: false)
        } catch {
            left = nil
        }
    }

    /// ADD: MY PRESETS read (their DUMPs, as connect left them) for the ones already there; the first ones that fit
    /// ticked (manager.js rsPresets).
    private func pickPresets() async {
        guard let o = offer, let u = unit else { return }
        if !u.dumpsLoaded { await u.reloadDumps() }
        let mine = u.actions.frames.filter { $0.key.hasPrefix("mine:") }.map(\.value)
        let marks = (try? await FizzDataShared.engine().importDuplicates(o.backup.presets, mine: mine)) ?? []
        fresh = o.backup.presets.indices.filter { $0 >= marks.count || marks[$0] != "mine" }
        room = max(0, nlib - u.mine.count)
        picked = Set(fresh.prefix(room))
    }

    // MARK: the writes

    /// The slide reached its end: the image (fitted for another package), then UnitActions.restore (safety backup,
    /// check pass, write pass); the progress screen and any failure are RestoreOverlay's.
    public func restore() async {
        guard let o = offer, var image = o.backup.image, let u = unit else { return }
        if let c = choice {
            do {
                let r = try await FizzDataShared.engine().restoreFit(image: image, unitSize: Int(u.storeInfo?.size ?? 0), choice: c, withImage: true)
                guard let img = r.image else { throw FizzDataError.bridge("restoreFit: no image") }
                image = img
                u.say("restore \(o.name): \(o.plan.from?.packageName ?? "?") -> \(o.plan.to?.packageName ?? "?") layout, \(c.projects.count) projects, \(c.presets.count) MY presets, \(r.copies) sounds as the projects' own copies")
            } catch {
                problem = "DOES NOT FIT"
                u.say("restore \(o.name): \(error)")
                return
            }
        }
        presented = false
        await run(image, o.name)
    }

    /// AGAIN (the FM-1 was playing): the same image once more.
    public func again() async {
        guard let l = last else { return }
        unit?.actions.dismissRestoreFailure()
        await run(l.image, l.name)
    }

    private func run(_ image: [UInt8], _ name: String) async {
        guard let u = unit else { return }
        last = (image, name)
        subject = nil
        highlight = nil
        _ = await u.actions.restore(image: image, name: name)
    }

    /// ADD n: the ticked presets stored (UnitActions.addPresets skips any now there); the count shown.
    public func add() async {
        guard let o = offer, let u = unit, canAdd else { return }
        adding = true
        let list = o.backup.presets.indices.filter { picked.contains($0) }.map { o.backup.presets[$0] }
        let r = await u.actions.addPresets(list)
        adding = false
        added = r
    }

    /// After FZER 11 (another store size): the same backup offered again, its presets only.
    public func offerPresetsOnly() async {
        guard let o = offer, let u = unit else { return }
        u.actions.dismissRestoreFailure()
        var p = o.plan
        p.mode = .presets
        p.fit = nil
        reset()
        offer = Offer(name: o.name, date: o.date, backup: o.backup, plan: p)
        presented = true
        await pickPresets()
    }

    /// The newest safety backup (kept before a restore): the way back after a write that was cut off.
    public var safety: BackupEntry? {
        unit?.backups.store.entries.first { $0.name.hasPrefix("FIZZ before") }
    }

    /// After a cut-off write: the safety backup, else the newest full one (manager.js: "restore the newest backup").
    public var wayBack: BackupEntry? { safety ?? unit?.backups.store.entries.first { $0.isFull } }
}
