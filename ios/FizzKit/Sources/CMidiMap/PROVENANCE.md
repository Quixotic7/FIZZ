# Provenance: CMidiMap

`RFMMidiMap.h` and `RFMMidiMap.cpp` come from RemoteFM1 for iOS (`RemoteFM1IOS`), commit **af91bc5**,
`RFMKit/Sources/RFMCore/`, MPL-2.0 at that commit (relicensed by the owner, their only author; earlier commits are
GPL-3.0-only). Adapted: the header includes FIZZ's `include/FizzMidiMap.h` instead of RemoteFM1's engine header, and
`rfme_fm1_candidate` (which used RemoteFM1's GPL `RemoteCandidates.h`) is left out; `rfm::MidiMapper` is unchanged.

`include/FizzMidiMap.h` (the map's numbers and the `fzmm_*` handle) and `FizzMidiMap.cpp` are FIZZ's own, MPL-2.0.
The full list of what PLAY's MIDI took from RemoteFM1: `../FizzUI/MIDI/PROVENANCE.md`.
