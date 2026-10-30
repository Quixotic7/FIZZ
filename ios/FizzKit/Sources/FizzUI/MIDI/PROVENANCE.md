# Provenance: PLAY's MIDI map, MIDI learn and the OMX-27

Some files here come from **RemoteFM1 for iOS** (`RemoteFM1IOS`, the owner's repository), commit **af91bc5**
("Licence: the MIDI map and learn, the OMX-27 and their tests are MPL-2.0"). At that commit these files are
MPL-2.0 (their SPDX lines say so; the owner, their only author, relicensed them for FIZZ for iOS). Earlier RemoteFM1
commits of the same files stay GPL-3.0-only. Nothing else of RemoteFM1 for iOS is used: its engine, `RemoteLink`,
`ControllerInput`, `AppMidi`, `MidiMapStore`, the faceplate (`PanelFeed`, `LearnMode`, `LearnViews`, `Toast`) and the
rest stay GPL and are not copied. Each copied file says in its header where it came from and what changed.

| File here | From (af91bc5) | |
|---|---|---|
| `OmxWire.swift` | `RFMKit/Sources/RFMKit/OMX27/OmxWire.swift` | unchanged (a provenance line added) |
| `OmxViews.swift` | `RFMKit/Sources/RFMKit/OMX27/OmxViews.swift` | unchanged (a provenance line added) |
| `OmxScreen.swift` | `RFMKit/Sources/RFMKit/OMX27/OmxScreen.swift` | unchanged (a provenance line added) |
| `OmxTransport.swift` | `RFMKit/Sources/RFMKit/OMX27/OmxTransport.swift` | adapted: CoreMIDI client and port names |
| `OmxLink.swift` | `RFMKit/Sources/RFMKit/OMX27/OmxLink.swift` | adapted: `pair` readable by `MidiHub` (its target is PLAY's panel) |
| `MidiMap.swift` | `RFMKit/Sources/RFMKit/MIDI/MidiMap.swift` | adapted: `import CMidiMap`; `apply(to:)` gives the map to FIZZ's mapper (`fzmm_adopt`) |
| `MidiLearn.swift` | `RFMKit/Sources/RFMKit/MIDI/MidiLearn.swift` | adapted: no engine (no `PanelEngineHost`, `rfme_learn`, `poll()`); `armed`; `take()` unchanged |
| `../../CMidiMap/RFMMidiMap.h` | `RFMKit/Sources/RFMCore/RFMMidiMap.h` | adapted: includes FIZZ's `FizzMidiMap.h` |
| `../../CMidiMap/RFMMidiMap.cpp` | `RFMKit/Sources/RFMCore/RFMMidiMap.cpp` | adapted: `rfme_fm1_candidate` (GPL `RemoteCandidates.h`) left out; `MidiMapper` unchanged |
| `../../../Tests/FizzUITests/OmxTests.swift` | `RFMKit/Tests/RFMKitTests/OmxTests.swift` | unchanged but for the module tested |
| `../../../Tests/FizzUITests/MidiMapTests.swift` | `RFMKit/Tests/RFMKitTests/MidiMapTests.swift` | adapted: FIZZ's store and controller input in place of RemoteFM1's; a map-file round trip |
| `../../../Tests/FizzUITests/MidiEngineTests.swift` | `RFMKit/Tests/RFMKitTests/MidiEngineTests.swift` | adapted: the same checks on FIZZ's `MidiRouter` instead of RemoteFM1's engine |

FIZZ's own (written for FIZZ, MPL-2.0): `MidiPanel.swift` (the panel's names the ported files use: `PanelFeed.Knob`
/ `Label`, `LEDLevel`, `RFMFonts`), `MidiMapStore.swift`, `MidiRouter.swift`, `MidiInput.swift`, `MidiHub.swift`,
`MidiAudioPage.swift`, `MidiLearnViews.swift`, `MidiDemo.swift`, and `CMidiMap/include/FizzMidiMap.h`,
`CMidiMap/FizzMidiMap.cpp`.

The stored map is RemoteFM1's format with its ids (`{"midiMap": [{src, target, mode?}], "knobSpeed": …}`, targets by
the FM-1's printed names), so a map file reads the same in both apps.
