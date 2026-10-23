/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* P1.3j, a diagnostic (macOS, CoreMIDI): every MIDI source's messages logged with a millisecond stamp, passively (a
 * second client on the same sources: the Standalone's own connection is untouched). The FM-1 running RemoteFM1 sends
 * its panel as MIDI: buttons as notes on channel 15 (0 OCT-, 1 OCT+, ..), the encoders as CC 16 + matrix encoder on
 * channel 15 (64 + detents; ALGORITHM is matrix encoder 1: "be 11 41" is one detent clockwise), MASTER as CC 7 / 39.
 *
 *     make -C tools midimon && tools/build/midimon log.txt 120     log two minutes into log.txt */
#include <CoreMIDI/CoreMIDI.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <mach/mach_time.h>
static FILE *out;
static void rd(const MIDIPacketList *pl, void *ref, void *src)
{
    const MIDIPacket *p = &pl->packet[0];
    unsigned i, k;
    (void)ref;
    for (i = 0; i < pl->numPackets; i++) {
        fprintf(out, "%llu %ld:", (unsigned long long)(mach_absolute_time() / 1000000ull), (long)src);
        for (k = 0; k < p->length && k < 24; k++)
            fprintf(out, " %02x", p->data[k]);
        fprintf(out, "\n");
        fflush(out);
        p = MIDIPacketNext(p);
    }
}
int main(int argc, char **argv)
{
    MIDIClientRef c;
    MIDIPortRef in;
    ItemCount n = MIDIGetNumberOfSources(), i;
    int secs = argc > 2 ? atoi(argv[2]) : 0;
    out = argc > 1 ? fopen(argv[1], "w") : stdout;
    MIDIClientCreate(CFSTR("fizzmon"), NULL, NULL, &c);
    MIDIInputPortCreate(c, CFSTR("in"), rd, NULL, &in);
    for (i = 0; i < n; i++) {
        MIDIEndpointRef e = MIDIGetSource(i);
        CFStringRef name = NULL;
        char b[256] = "";
        MIDIObjectGetStringProperty(e, kMIDIPropertyDisplayName, &name);
        if (name)
            CFStringGetCString(name, b, sizeof b, kCFStringEncodingUTF8);
        fprintf(out, "source %lu: %s\n", (unsigned long)i, b);
        MIDIPortConnectSource(in, e, (void *)(long)i);
    }
    fflush(out);
    if (secs > 0)
        sleep((unsigned)secs);
    return 0;
}
