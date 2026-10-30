// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// ▶ play a backup (mock-up 44; the web's site/js/backup-play.js backupFlash): a full backup's store image written
// again as the emulated FIZZ's whole flash (StoreImage.hostFlash: the same records, the host's layout), booted in a
// throwaway FizzEmulator with no flash file (EmulatorHost.playBackup), PLAY opened on it. THIS iPHONE's own flash
// file is never written. PLAY's chip turns into the yellow pill, BACKUP · 28 OCT; its × (or a pick in the unit picker)
// throws the copy away and THIS iPHONE takes the core back.

import Foundation
import FizzData
import FizzEngine
import SwiftUI

/// The backup PLAY plays.
public struct BackupPlaying: @unchecked Sendable, Equatable, Identifiable {
    public let id = UUID()
    /// Its throwaway FIZZ (no flash file).
    public let emulator: FizzEmulator
    /// The backup's file name in Backups/.
    public let name: String
    public let date: Date

    /// "BACKUP · 28 OCT"
    public var title: String { "BACKUP · \(Self.day(date))" }

    /// "28 OCT"
    static func day(_ d: Date) -> String {
        let f = DateFormatter()
        f.locale = Locale(identifier: "en")
        f.dateFormat = "d MMM"
        return f.string(from: d).uppercased()
    }

    public static func == (a: Self, b: Self) -> Bool { a.id == b.id }
}

extension BackupModel {
    /// The backup PLAY plays now (its row ringed).
    public var playingName: String? { unit?.emulator?.backup?.name }

    /// ▶ offered: a full backup, and an emulated FIZZ to play it on.
    public func canPlay(_ e: BackupEntry) -> Bool { e.isFull && unit?.emulator != nil }

    /// ▶: the backup's store booted in a throwaway FIZZ and PLAY opened on it. A row already playing: PLAY again.
    /// false: it could not be read (NOT A BACKUP) or its store fits no FIZZ (CAN'T PLAY).
    @discardableResult public func play(_ e: BackupEntry) async -> Bool {
        guard canPlay(e), let u = unit, let host = u.emulator else { return false }
        if host.backup?.name == e.name { u.goTo = .play; return true }
        let flash: [UInt8]
        do {
            flash = try await Self.flash(of: store.url(of: e))
        } catch {
            u.say("play \(e.name): \(error)")
            reportPlayFailure(error is StoreImage.Failure ? .cannotPlay : .notABackup)
            return false
        }
        await host.playBackup(flash: flash, name: e.name, date: e.date)
        guard host.backup?.name == e.name else { return false }
        u.say("PLAY on \(e.name): a throwaway FIZZ (\(EmulatorHost.hereName)'s flash untouched)")
        u.goTo = .play
        return true
    }

    /// The core's whole flash from a backup file (fizzbak.js readBackup's image, then convertImage to the host's).
    static func flash(of url: URL) async throws -> [UInt8] {
        let bytes = try [UInt8](Data(contentsOf: url))
        let s = try await FizzDataShared.engine().readBackup(bytes, fileName: url.lastPathComponent)
        guard let img = s.image else { throw StoreImage.Failure.notStore }
        return try StoreImage.hostFlash(from: img)
    }
}

/// PLAY's chip on a backup (mock-up 54): yellow, BACKUP · 28 OCT in the chip's place and height, then × in a black
/// ring (back to THIS iPHONE). The pill itself opens the unit picker, as the chip does.
struct BackupPill: View {
    let backup: BackupPlaying
    /// the chip's height and type size
    let height: CGFloat
    let font: CGFloat
    let pick: () -> Void
    let close: () -> Void

    var body: some View {
        let k = height / 28
        HStack(spacing: 0) {
            Button(action: pick) {
                Text(backup.title).dm(font - 1, 600).foregroundStyle(RGB(0x1b1300).color).lineLimit(1).fixedSize()
                    .padding(.leading, 11 * k).padding(.trailing, 7 * k)
                    .frame(height: height)
                    .contentShape(Rectangle())
            }
            .buttonStyle(.plain)
            Button(action: close) {
                IconView(TransferIcons.close, color: FZ.yellow.color, lineWidth: 3.4)
                    .frame(width: 9 * k, height: 9 * k)
                    .frame(width: 22 * k, height: 22 * k)
                    .background(Circle().fill(.black.opacity(0.82)))
                    .padding(.leading, 2 * k).padding(.trailing, 3 * k)
                    .frame(height: height)
                    .contentShape(Rectangle())
            }
            .buttonStyle(PressStyle(scale: 0.88))
            .accessibilityLabel("Back to \(EmulatorHost.hereName)")
        }
        .background(Capsule().fill(RGB(0xFFD23F).color))
        .overlay(Capsule().strokeBorder(RGB(0x22222a).color, lineWidth: 1.5 * k))
        .fixedSize()
    }
}

/// ▶ on a backup's row, in the emulator's colour: a ring, or filled and pulsing while that backup plays.
struct BackupPlayButton: View {
    let on: Bool
    let action: () -> Void

    var body: some View {
        let c = PlayTheme.saved.accent
        Button(action: action) {
            Animated(still: 0.5, active: on) { t in
                let k = on ? (t.truncatingRemainder(dividingBy: 1.6)) / 1.6 : 0
                ZStack {
                    Circle().fill(on ? c.color : c.opacity(0.16))
                    Circle().strokeBorder(c.color, lineWidth: 2)
                    IconView(TransferIcons.play, color: on ? .black : c.color, lineWidth: 0, backdrop: .clear, fill: true)
                        .frame(width: 15, height: 15).offset(x: 1.5)
                    if on {
                        Circle().strokeBorder(c.color, lineWidth: 2).scaleEffect(1 + 0.45 * k).opacity(1 - k)
                    }
                }
                .frame(width: 42, height: 42)
                .shadow(color: on ? c.opacity(0.6) : .clear, radius: 10)
            }
            .frame(width: 46, height: 48)
            .contentShape(Rectangle())
        }
        .buttonStyle(PressStyle(scale: 0.9))
        .accessibilityLabel("Play this backup")
    }
}
