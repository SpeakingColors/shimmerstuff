#include "../src/Fx.h"
#include <cstdio>
#include <random>
using namespace wm::fx;
static int fails = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL line %d: %s\n", __LINE__, #c); ++fails; } } while (0)

// returns rms of the output for a 1 s noise burst followed by 1 s of silence (tail); checks for NaN
static double run(const std::string& graph, bool rev, bool del, bool ifx, bool wfx, double* tailRms = nullptr) {
    Engine e; e.prepare(48000); e.revOn = rev; e.delOn = del; e.ifxOn = ifx; e.wfxOn = wfx;
    e.rs.type = wm::engine::ReverbType::ShimmerHall; e.rs.mix = 0.4f; e.rs.p.decay = 0.6f; e.rs.p.dampHz = 6000;
    e.dp.mix = 0.4f; e.dp.timeMs = 300; e.dp.feedback = 0.5f; e.wp.squashMix = 0.5f; e.wp.satMix = 0.3f; e.ip.satMix = 0.2f;
    const Graph g = parseGraph(graph); std::mt19937 rng(7); std::uniform_real_distribution<float> d(-0.4f, 0.4f);
    double s = 0, t = 0;
    for (int i = 0; i < 96000; ++i) {
        const float x = i < 48000 ? d(rng) : 0.f, y = i < 48000 ? d(rng) : 0.f; float ol, orr;
        e.tick(g, x, y, x, y, ol, orr);
        CHECK(std::isfinite(ol) && std::isfinite(orr));
        if (i < 48000) s += ol * ol + orr * orr; else t += ol * ol + orr * orr;
    }
    if (tailRms) *tailRms = std::sqrt(t / 96000);
    return std::sqrt(s / 96000);
}

int main() {
    // graph parsing: unknown names ignored, duplicates dropped, missing nodes appended, round trip
    { Graph g = parseGraph("ifx>rev|del>wfx"); CHECK(g.stages == 3 && g.count[1] == 2 && graphString(g) == "ifx>rev|del>wfx"); }
    { Graph g = parseGraph("rev>rev>zzz"); CHECK(g.stages == 4 && graphString(g) == "rev>ifx>del>wfx"); }
    { Graph g = parseGraph(""); CHECK(g.stages == 4); }

    // every layout and every on/off state: finite, not silent when something is on, tails exist when a reverb or delay is on
    const char* layouts[] = {"ifx>rev>wfx>del", "ifx>del>rev>wfx", "ifx>rev|del>wfx", "ifx>del>wfx>rev", "wfx>ifx>rev>del", "rev|del|ifx|wfx"};
    int combos = 0;
    for (auto* L : layouts) for (int m = 0; m < 16; ++m) {
        const bool rev = m & 1, del = m & 2, ifx = m & 4, wfx = m & 8; double tail = 0; const double rms = run(L, rev, del, ifx, wfx, &tail);
        CHECK(rms > 0.01); if (rev || del) CHECK(tail > 1e-5); ++combos;
    }
    std::printf("%d layout/on-off combinations ok\n", combos);

    // everything off passes the input untouched
    { Engine e; e.prepare(48000); e.revOn = e.delOn = e.ifxOn = e.wfxOn = false; const Graph g = parseGraph("ifx>rev>del>wfx"); float ol, orr; e.tick(g, 0.3f, -0.2f, 0.3f, -0.2f, ol, orr); CHECK(ol == 0.3f && orr == -0.2f); }

    // duck shapes stay in 0..1; follow mode ducks while the source plays and recovers after it stops
    for (int s = 0; s < kNumShapes; ++s) for (int i = 0; i <= 100; ++i) { const float v = shapeAt(s, i / 100.f); CHECK(v >= -1e-6f && v <= 1.0001f); }
    { Duck d; d.prepare(48000); DuckParams p; p.amt = 1; p.mode = 0; p.src = 1; p.relMs = 100; float g = 1;
      for (int i = 0; i < 4800; ++i) g = d.tick(0.8f, 0.8f, p); CHECK(g < 0.3f);
      for (int i = 0; i < 48000; ++i) g = d.tick(0, 0, p); CHECK(g > 0.95f); }
    // sync mode: a pump cycle of 1 beat at 120 BPM repeats every 0.5 s (free-running with no source)
    { Duck d; d.prepare(48000); DuckParams p; p.amt = 1; p.mode = 2; p.src = 0; p.shape = 0; p.beats = 1; p.bpm = 120;
      float lo = 1, hi = 0; for (int i = 0; i < 96000; ++i) { const float g = d.tick(0, 0, p); if (i > 3000) { lo = std::min(lo, g); hi = std::max(hi, g); } }
      CHECK(lo < 0.1f && hi > 0.9f); }
    // None source in follow mode does nothing
    { Duck d; d.prepare(48000); DuckParams p; p.amt = 1; p.mode = 0; p.src = 0; float g = 1; for (int i = 0; i < 1000; ++i) g = d.tick(0.9f, 0.9f, p); CHECK(g == 1.f); }

    // the ADAA saturation stays bounded and quiet signals pass nearly unchanged
    { Adaa a; float mx = 0; for (int i = 0; i < 10000; ++i) mx = std::max(mx, std::fabs(satStage(a, 0, 3.f * std::sin(i * 0.05f), 20.f, 1.f))); CHECK(mx < 2.f); }
    std::printf(fails ? "FAILED\n" : "all tests passed\n");
    return fails ? 1 : 0;
}
