// SPDX-License-Identifier: GPL-3.0-only
// A MIDI keyboard into FIZZ (0.9.5: a channel plays the tracks set to it; channel 16 always plays the selected track).
// With "the selected track" on (the default), the keyboard's channel messages (notes, pressure, controllers, program,
// pitch bend) go to FIZZ on channel 16, whatever channel the keyboard sends; system messages pass as they are.
// No DOM: the tests run it in Node.
export const SELECTED_TRACK_CH = 16;
export function toSelectedTrack(bytes) {
  const d = Uint8Array.from(bytes);
  if (d.length && d[0] >= 0x80 && d[0] < 0xf0) d[0] = (d[0] & 0xf0) | (SELECTED_TRACK_CH - 1);
  return d;
}
