// SPDX-License-Identifier: GPL-3.0-only
// SYNTHETIC Digitone .dnsnd files for the tests (never a real preset: the owner's files are only ever read in place).
//   dnsndV4(name) -> Uint8Array: the shape of a sound saved on Digitone OS 1.42 that FIZZ's importer refused as "a
//     newer Digitone format (version 4)": a record of version 4 in version 5's layout (the tail at version 5's offsets),
//     the payload header "0095", the container's object version 4, manifest FirmwareVersion "1.42", the tag KICK.
//     Built from FIZZ's own writer (js/fizz2dn.js: a version-5 record of FM TONE's defaults), its version byte set to 4.
import { dn1Record, container, zipTransfer, manifestText, defaults } from "../js/fizz2dn.js";

export function dnsndV4(name = "SYNTH KICK") {
  const v = defaults();
  v[0] = 3;                                         // (ALGO 4, something not the default to see come back)
  const record = dn1Record(v, { name, tagBits: 1 }, []);
  record[7] = 4;                                    // the record's version: 4
  const payload = container({ device: 9, build: "0095", objVersion: 4, body: record });
  const pl = name.slice(0, 15).trim();
  const man = { FormatVersion: "1.0", ProductType: ["24", "30"], Payload: pl, FileType: "Sound", FirmwareVersion: "1.42", MetaInfo: { Tags: ["KICK"] } };
  return zipTransfer([["manifest.json", manifestText(man)], [pl, payload]], new Date(2026, 9, 24));
}
