// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// The app's shared frame pieces (UX pass): one sheet (FizzSheet: header, scrolling content, actions pinned at the
// bottom; it opens at its content's height), one confirm (ConfirmCard, standalone over the whole screen, sheets and
// the tab bar included), a pressed state for tiles and rows, the room the phone's floating bar takes, and a sideways
// pan that never starts on a vertical swipe (rows in a scrolling list).

import SwiftUI
#if canImport(UIKit)
import UIKit
#endif

// MARK: - The sheet

/// A sheet's colour.
let sheetBackground = RGB(0x0d0d11)

/// How a FizzSheet opens.
enum SheetSize: Equatable {
    /// at its content's height (header + content + actions), pulled up to large
    case fit
    /// half height, pulled up to large (long lists)
    case medium
    /// full height only
    case large
}

/// The standard sheet: `header` pinned at the top, `content` scrolling, `actions` pinned at the bottom above the home
/// indicator (always reachable). Apply nothing else to the sheet's root: it sets its own detents, background, corner
/// radius and drag indicator.
struct FizzSheet<Header: View, Content: View, Actions: View>: View {
    var size: SheetSize = .fit
    /// room on the sides
    var inset: CGFloat = 20
    let header: Header
    let content: Content
    let actions: Actions

    @Environment(\.fizzSnapshot) private var snapshot
    @State private var parts = Parts()
    @State private var detent: PresentationDetent = .medium

    struct Parts: Equatable { var header: CGFloat = 0, content: CGFloat = 0, actions: CGFloat = 0 }

    init(size: SheetSize = .fit, inset: CGFloat = 20, @ViewBuilder header: () -> Header, @ViewBuilder content: () -> Content,
         @ViewBuilder actions: () -> Actions) {
        self.size = size
        self.inset = inset
        self.header = header()
        self.content = content()
        self.actions = actions()
        _detent = State(initialValue: size == .large ? .large : .medium)
    }

    /// the content's whole height (the fitted detent); the sheet's own bottom safe area is not counted
    private var fitted: CGFloat { ceil(parts.header + parts.content + parts.actions) }

    var body: some View {
        if snapshot {
            VStack(alignment: .leading, spacing: 0) {
                header.padding(.bottom, 14)
                content
                actions.padding(.top, 14)
            }
        } else {
            sheet
        }
    }

    private var sheet: some View {
        VStack(spacing: 0) {
            header
                .frame(maxWidth: .infinity, alignment: .leading)
                .padding(.horizontal, inset).padding(.top, 26).padding(.bottom, 14)
                .onGeometryChange(for: CGFloat.self) { $0.size.height } action: { parts.header = $0 }
            ScrollView {
                content
                    .frame(maxWidth: .infinity, alignment: .leading)
                    .padding(.horizontal, inset).padding(.bottom, 6)
                    .onGeometryChange(for: CGFloat.self) { $0.size.height } action: { parts.content = $0 }
            }
            .scrollIndicators(.hidden)
            .scrollBounceBehavior(.basedOnSize)
            .scrollDismissesKeyboard(.interactively)
            actions
                .frame(maxWidth: .infinity, alignment: .leading)
                .padding(.horizontal, inset).padding(.top, 12).padding(.bottom, 10)
                .background(alignment: .top) {
                    // (content scrolling under the actions fades out)
                    LinearGradient(colors: [sheetBackground.color.opacity(0), sheetBackground.color], startPoint: .top, endPoint: .bottom)
                        .frame(height: 16).offset(y: -16).allowsHitTesting(false)
                }
                .background(sheetBackground.color)
                .onGeometryChange(for: CGFloat.self) { $0.size.height } action: { parts.actions = $0 }
        }
        .background(sheetBackground.color.ignoresSafeArea())
        .presentationDetents(detents, selection: $detent)
        .presentationDragIndicator(.visible)
        .presentationCornerRadius(30)
        .presentationBackground(sheetBackground.color)
        .preferredColorScheme(.dark)
        .onChange(of: fitted, initial: true) { _, new in
            guard size == .fit, new > 0 else { return }
            // (opened, or grown / shrunk while at the fitted height: follow it; pulled up to large: stay)
            if detent != .large { detent = .height(new) }
        }
    }

    private var detents: Set<PresentationDetent> {
        switch size {
        case .fit: fitted > 0 ? [.height(fitted), .large] : [.medium, .large]
        case .medium: [.medium, .large]
        case .large: [.large]
        }
    }
}

extension FizzSheet where Actions == EmptyView {
    /// A sheet with no pinned actions.
    init(size: SheetSize = .fit, inset: CGFloat = 20, @ViewBuilder header: () -> Header, @ViewBuilder content: () -> Content) {
        self.init(size: size, inset: inset, header: header, content: content, actions: { EmptyView() })
    }
}

// MARK: - The confirm

/// ui.js confirmBox as a card over a scrim: the title in its colour, what, a short line, UNDO KEPT / CLEARED, the
/// button and CANCEL. Shown with `.confirmCard($ask)`: over the whole screen (above sheets and the tab bar).
struct ConfirmCard: View {
    let ask: ConfirmAsk
    /// the card shown (it slides in and out by itself)
    var shown = true
    let cancel: () -> Void
    let go: () -> Void

    var body: some View {
        ZStack(alignment: .bottom) {
            Color.black.opacity(shown ? 0.62 : 0).ignoresSafeArea().onTapGesture(perform: cancel)
            if shown { card.transition(.move(edge: .bottom).combined(with: .opacity)) }
        }
    }

    private var card: some View {
        VStack(alignment: .leading, spacing: 14) {
            HStack(spacing: 10) {
                Text(ask.title).fz(14, 800, em: 0.12).foregroundStyle(ask.color.color).lineLimit(1).minimumScaleFactor(0.7)
                Spacer(minLength: 6)
                let c = ask.undo == .kept ? FZ.muted : FZ.orange
                Text(ask.undo == .kept ? "UNDO KEPT" : "UNDO CLEARED").fz(12, 800, em: 0.08).foregroundStyle(c.color)
                    .padding(.horizontal, 8).frame(height: 24)
                    .overlay(Capsule().strokeBorder(c.color, lineWidth: 1.5))
                    .fixedSize()
            }
            Text(ask.what).fz(32, 800, em: -0.01).foregroundStyle(FZ.text.color).lineLimit(2).minimumScaleFactor(0.5)
            if let d = ask.detail {
                Text(d).fz(15, 800, em: 0.04).foregroundStyle(FZ.body.color).lineLimit(2).minimumScaleFactor(0.8)
            }
            HStack(spacing: 10) {
                PillButton(title: "CANCEL", ghost: true, height: 56, action: cancel)
                PillButton(title: ask.go, color: ask.color, height: 56, action: go)
            }
            .padding(.top, 4)
        }
        .padding(.horizontal, 20).padding(.top, 20).padding(.bottom, 22)
        .frame(maxWidth: 520)
        .background(RoundedRectangle(cornerRadius: 28).fill(sheetBackground.color))
        .overlay(RoundedRectangle(cornerRadius: 28).strokeBorder(ask.color.mix(60, FZ.line).color, lineWidth: 1.5))
        .shadow(color: ask.color.opacity(0.25), radius: 30)
        .padding(.horizontal, 12).padding(.bottom, 12)
    }
}

extension View {
    /// The confirm card while `ask` is set: standalone, over the whole screen (a sheet's too), never under the tab
    /// bar. GO closes the card and runs the ask's action at once.
    func confirmCard(_ ask: Binding<ConfirmAsk?>) -> some View { modifier(ConfirmPresenter(ask: ask)) }
}

private struct ConfirmPresenter: ViewModifier {
    @Binding var ask: ConfirmAsk?
    @Environment(\.fizzSnapshot) private var snapshot
    /// what the cover shows (set with no system animation: the card animates itself)
    @State private var presented: ConfirmAsk?
    @State private var shown = false

    func body(content: Content) -> some View {
        if snapshot {
            // (ImageRenderer draws no covers: the card over the screen)
            content.overlay { if let a = ask { ConfirmCard(ask: a, cancel: {}, go: {}) } }
        } else {
            content
                #if os(iOS)
                .fullScreenCover(item: $presented) { a in
                    ConfirmCard(ask: a, shown: shown, cancel: close) {
                        close()
                        a.action()
                    }
                    .presentationBackground(.clear)
                    .onAppear { withAnimation(.spring(response: 0.32, dampingFraction: 0.86)) { shown = true } }
                }
                #else
                .overlay {
                    if let a = presented { ConfirmCard(ask: a, shown: shown, cancel: close) { close(); a.action() } }
                }
                #endif
                .onChange(of: ask?.id, initial: true) { _, id in
                    if id != nil, presented?.id != id {
                        shown = false
                        withTransaction(Self.still) { presented = ask }
                        #if !os(iOS)
                        withAnimation(.spring(response: 0.32, dampingFraction: 0.86)) { shown = true }
                        #endif
                    } else if id == nil, presented != nil {
                        dismiss()
                    }
                }
        }
    }

    private static var still: Transaction {
        var t = Transaction()
        t.disablesAnimations = true
        return t
    }

    /// (the ask cleared: onChange takes the card away)
    private func close() { ask = nil }

    /// The cover goes at once, no system slide: a cover still fading would swallow the next tap on the screen.
    private func dismiss() {
        withTransaction(Self.still) {
            shown = false
            presented = nil
        }
    }
}

// MARK: - Pressed state

/// A tile or a row pressed: it sinks a little and dims; acts on touch-up at once (a Button, so a scroll wins).
struct PressStyle: ButtonStyle {
    var scale: CGFloat = 0.97
    func makeBody(configuration: Configuration) -> some View {
        configuration.label
            .scaleEffect(configuration.isPressed ? scale : 1)
            .brightness(configuration.isPressed ? 0.08 : 0)
            .animation(.spring(response: 0.2, dampingFraction: 0.7), value: configuration.isPressed)
    }
}

// MARK: - The floating bar's room

private struct BarClearanceKey: EnvironmentKey { static let defaultValue: CGFloat = 0 }

public extension EnvironmentValues {
    /// The height the phone's floating bars cover at the bottom of a tab (the tab bar, KEEP / CANCEL when it shows);
    /// zero on the iPad. Scrolling content ends this far up, bars of a screen's own sit on top of it.
    var fizzBarClearance: CGFloat {
        get { self[BarClearanceKey.self] }
        set { self[BarClearanceKey.self] = newValue }
    }
}

// MARK: - A sideways pan

extension View {
    /// A horizontal pan that never starts on a mostly vertical swipe, so the list around it always scrolls (iOS 18:
    /// a UIKit pan; before, SwiftUI's drag, which did not block the scroll there). `changed` gets the translation,
    /// `ended` the translation and the speed (nil: cancelled).
    @ViewBuilder
    func sidewaysPan(enabled: Bool, changed: @escaping (CGFloat) -> Void, ended: @escaping (CGFloat?, CGFloat) -> Void) -> some View {
        if !enabled {
            self
        } else {
            #if os(iOS)
            if #available(iOS 18.0, *) {
                gesture(SidewaysPan(changed: changed, ended: ended))
            } else {
                simultaneousGesture(LegacySidewaysDrag(changed: changed, ended: ended).gesture)
            }
            #else
            simultaneousGesture(LegacySidewaysDrag(changed: changed, ended: ended).gesture)
            #endif
        }
    }
}

/// SwiftUI's drag, sideways moves only (iOS 17, macOS).
@MainActor private struct LegacySidewaysDrag {
    let changed: (CGFloat) -> Void
    let ended: (CGFloat?, CGFloat) -> Void
    var gesture: some Gesture {
        DragGesture(minimumDistance: 14)
            .onChanged { v in
                guard abs(v.translation.width) > abs(v.translation.height) else { return }
                changed(v.translation.width)
            }
            .onEnded { v in
                guard abs(v.translation.width) > abs(v.translation.height) else { ended(nil, 0); return }
                ended(v.translation.width, v.predictedEndTranslation.width - v.translation.width)
            }
    }
}

#if os(iOS)
@available(iOS 18.0, *)
private struct SidewaysPan: UIGestureRecognizerRepresentable {
    let changed: (CGFloat) -> Void
    let ended: (CGFloat?, CGFloat) -> Void

    func makeCoordinator(converter: CoordinateSpaceConverter) -> Coordinator { Coordinator() }

    func makeUIGestureRecognizer(context: Context) -> UIPanGestureRecognizer {
        let g = UIPanGestureRecognizer()
        g.maximumNumberOfTouches = 1
        g.delegate = context.coordinator
        return g
    }

    func handleUIGestureRecognizerAction(_ g: UIPanGestureRecognizer, context: Context) {
        let t = g.translation(in: g.view).x
        switch g.state {
        case .began, .changed: changed(t)
        case .ended: ended(t, g.velocity(in: g.view).x * 0.25)
        case .cancelled, .failed: ended(nil, 0)
        default: break
        }
    }

    final class Coordinator: NSObject, UIGestureRecognizerDelegate {
        /// only a mostly sideways start
        func gestureRecognizerShouldBegin(_ g: UIGestureRecognizer) -> Bool {
            guard let p = g as? UIPanGestureRecognizer else { return true }
            let v = p.velocity(in: p.view), t = p.translation(in: p.view)
            let dx = abs(v.x) + abs(t.x) * 20, dy = abs(v.y) + abs(t.y) * 20
            return dx > dy * 1.3
        }
    }
}
#endif
