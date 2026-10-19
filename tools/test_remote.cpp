// SPDX-License-Identifier: GPL-3.0-only
// P1.3j: QUADRILLE through FM1VST's own processor and Remote FM-1 path, headless: FM1Processor with Remote on, against
// libremotefm1's in-process fake unit (FM1EMU_REMOTE_FAKE=1: nothing on USB or CoreMIDI is opened). The unit's buttons
// and encoders are injected on the fake as the real unit sends them (matrix ids), so they go through RemoteLink, the
// processor's applyRemote, Device::enc and the glue, as on the owner's Mac. Checked: F3 opened on the unit (OCT- and
// OCT+ together, let go; and held while turning), the ALGORITHM encoder moves the tempo, SHIFT (GLO) + ALGORITHM the
// swing, PRESETS in F3 does nothing, and the unit's screen shows it (the fake's shadow equals panel->lcd).
//
//     make -C tools remote        (builds with FM1VST's flags and libraries from FM1VST_DIR/build-quadrille)
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>

#include "Processor.h"
#include "rfm/fake.h"

using Clock = std::chrono::steady_clock;
static int fails = 0;
static void check(bool c, const std::string &what)
{
    std::printf("  %s  %s\n", c ? "ok  " : "FAIL", what.c_str());
    fails += !c;
}
struct Host {
    FM1Processor &p;
    juce::AudioBuffer<float> buf;
    static constexpr int kBs = 480;
    Host(FM1Processor &proc) : p(proc), buf(2, kBs)
    {
        p.setRateAndBufferSizeDetails(48000.0, kBs);
        p.prepareToPlay(48000.0, kBs);
    }
    void tick()
    {
        const auto t = Clock::now();
        juce::MidiBuffer m;
        buf.clear();
        p.processBlock(buf, m);
        std::this_thread::sleep_until(t + std::chrono::milliseconds(10));
    }
    void runMs(int ms)
    {
        const auto end = Clock::now() + std::chrono::milliseconds(ms);
        while (Clock::now() < end)
            tick();
    }
};

static std::string info(FM1Processor &p)
{
    char b[512];
    p.deviceForTest()->core()->ui_info(b, sizeof b);
    return b;
}
static int bpmOf(const std::string &s)
{
    const size_t i = s.find(" BPM");
    size_t j = i;
    while (j > 0 && s[j - 1] >= '0' && s[j - 1] <= '9')
        j--;
    return i == std::string::npos ? -1 : std::atoi(s.c_str() + j);
}

int main(int argc, char **argv)
{
    const char *home = argc > 1 ? argv[1] : "/tmp/fm1-remote-quadrille";
    std::string mk = std::string("mkdir -p '") + home + "'";
    if (std::system(mk.c_str()) != 0)
        return 2;
    setenv("FM1EMU_HOME", home, 1);
    setenv("FM1EMU_REMOTE_FAKE", "1", 1);
    juce::ScopedJuceInitialiser_GUI gui;
    FM1Processor p;
    RemoteLink *rl = p.remoteForTest();
    if (!rl || rl->backend() != RemoteLink::Backend::Fake) {
        std::printf("FAIL: not the fake unit\n");
        return 1;
    }
    Host h(p);
    h.runMs(300);
    p.setRemoteEnabled(true);
    for (int i = 0; i < 500 && rl->status().state != RemoteLink::kConnected; i++)
        h.tick();
    h.runMs(1500);
    const fm1panel_t *H = p.deviceForTest()->panel();
    std::printf("test_remote: %s %s through FM1Processor and the fake unit\n", p.deviceForTest()->core()->name,
                p.deviceForTest()->core()->version);
    auto btn = [&](int label, int on) {
        const uint32_t id = H->btn_id[label];
        rl->withFake([id, on](rfm_fake_t *f) { rfm_fake_button(f, id, on); });
        h.runMs(60);
    };
    auto enc = [&](int role, int d) {
        const int e = H->enc_id[role];
        rl->withFake([e, d](rfm_fake_t *f) { rfm_fake_enc(f, (uint32_t)e, d); });
        h.runMs(250);
    };
    auto shadowSame = [&]() {
        bool same = false;
        for (int k = 0; k < 40 && !same; k++) {
            rl->withFake([&](rfm_fake_t *f) {
                same = std::memcmp(rfm_fake_shadow(f), H->lcd, (size_t)RFM_W * RFM_H * 2) == 0;
            });
            if (!same)
                h.runMs(50);
        }
        return same;
    };
    btn(EMU_B_HOME, 1), btn(EMU_B_HOME, 0);
    btn(EMU_B_OCTDN, 1), btn(EMU_B_OCTUP, 1), btn(EMU_B_OCTUP, 0), btn(EMU_B_OCTDN, 0);
    int b0 = bpmOf(info(p));
    enc(EMU_E_ALGO, 7);
    check(bpmOf(info(p)) == b0 + 7 && shadowSame(), "F3 opened on the unit, ALGORITHM +7 on the unit: the tempo " +
                                                         std::to_string(b0) + " -> " + std::to_string(bpmOf(info(p))) +
                                                         ", the unit's screen shows it");
    b0 = bpmOf(info(p));
    btn(EMU_B_GLO, 1);
    enc(EMU_E_ALGO, 3);
    btn(EMU_B_GLO, 0);
    check(bpmOf(info(p)) == b0, "GLO + ALGORITHM: the swing, not the tempo (" + std::to_string(bpmOf(info(p))) + ")");
    const std::string before = info(p);
    enc(EMU_E_PRESETS, 2);
    check(info(p) == before, "PRESETS in F3: nothing (" + info(p) + ")");
    btn(EMU_B_OCTDN, 1), btn(EMU_B_OCTDN, 0);
    btn(EMU_B_OCTDN, 1), btn(EMU_B_OCTUP, 1);           /* both held while turning (F3 as it was before the latch) */
    b0 = bpmOf(info(p));
    enc(EMU_E_ALGO, -4);
    btn(EMU_B_OCTUP, 0), btn(EMU_B_OCTDN, 0);
    check(bpmOf(info(p)) == b0 - 4, "OCT- and OCT+ held, ALGORITHM -4: the tempo " + std::to_string(b0) + " -> " +
                                        std::to_string(bpmOf(info(p))));
    p.setRemoteEnabled(false);
    h.runMs(300);
    std::printf("test_remote: %d failed\n", fails);
    return fails ? 1 : 0;
}
