#include "../src/Fx.h"
#include <cstdio>
#include <random>
using namespace wm::fx;
static int fails = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL line %d: %s\n", __LINE__, #c); ++fails; } } while (0)

// returns rms of the output for a 1 s noise burst followed by 1 s of silence (tail); checks for NaN
static double run(const std::string& graph, bool rev, bool del, bool ifx, bool wfx, double* tailRms = nullptr) {
    Engine e; e.prepare(48000); e.revOn = rev; e.delOn = del; e.ifxOn = ifx; e.wfxOn = wfx;
    e.rs.type = wm::engine::ReverbType::Hall; e.rs.shimmer = true; e.rs.mix = 0.4f; e.rs.p.decay = 0.6f; e.rs.p.dampHz = 6000;
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
    { Graph g = parseGraph("ifx>rev|del>wfx>mod"); CHECK(g.stages == 4 && g.count[1] == 2 && graphString(g) == "ifx>rev|del>wfx>mod"); }
    { Graph g = parseGraph("rev>rev>zzz"); CHECK(g.stages == 5 && graphString(g) == "rev>ifx>del>wfx>mod"); }
    { Graph g = parseGraph(""); CHECK(g.stages == 5); }

    // every layout and every on/off state: finite, not silent when something is on, tails exist when a reverb or delay is on
    const char* layouts[] = {"ifx>rev>wfx>del", "ifx>del>rev>wfx", "ifx>rev|del>wfx", "ifx>del>wfx>rev", "wfx>ifx>rev>del", "rev|del|ifx|wfx"};
    int combos = 0;
    for (auto* L : layouts) for (int m = 0; m < 16; ++m) {
        const bool rev = m & 1, del = m & 2, ifx = m & 4, wfx = m & 8; double tail = 0; const double rms = run(L, rev, del, ifx, wfx, &tail);
        CHECK(rms > 0.01); if (rev || del) CHECK(tail > 1e-5); ++combos;
    }
    std::printf("%d layout/on-off combinations ok\n", combos);
    // modulation: every type finite, bounded, audible; routing with the mod box parses and round-trips
    {
        const char* nm[8] = {"Chorus", "Flanger", "Phaser", "Tremolo", "Pan", "Rotary", "Vibrato", "Detune"};
        for (int t = 0; t < 8; ++t) {
            ModFx m; m.prepare(48000); ModParams p; p.type = t; p.speed = t == 5 ? 6.0f : 1.5f; p.depth = 0.8f; p.regen = 0.4f; p.voices = 3; p.diffusion = 0.4f; p.shift = 20;
            double e = 0, d = 0, pk = 0; bool fin = true;
            for (int i = 0; i < 96000; ++i) { const float x = 0.5f * std::sin(0.03f * float(i)) + 0.2f * std::sin(0.31f * float(i)); m.process(x, x * 0.8f, p); fin = fin && std::isfinite(m.oL) && std::isfinite(m.oR); pk = std::max(pk, (double)std::max(std::fabs(m.oL), std::fabs(m.oR))); if (i > 48000) { e += m.oL * m.oL; d += (m.oL - x) * (m.oL - x); } }
            CHECK(fin && pk < 3.0 && e > 1.0 && d > 1.0); std::printf("  %s: peak %.2f, differs from dry %.0f %%\n", nm[t], pk, 100.0 * d / e);
        }
        { const Graph g = parseGraph("ifx>rev>del>mod>wfx"); CHECK(g.stages == 5 && graphString(g) == "ifx>rev>del>mod>wfx"); const Graph g2 = parseGraph("ifx>rev>del>wfx"); CHECK(graphString(g2) == "ifx>rev>del>wfx>mod"); }   // old saves get the box at the end
        { Engine e; e.prepare(48000); e.revOn = e.delOn = e.ifxOn = e.wfxOn = false; e.modOn = true; e.modUse[0] = false; e.modUse[3] = true; e.mps[3].type = 3; e.mps[3].depth = 1.0f; e.mps[3].speed = 2.0f; const Graph g = parseGraph("ifx>rev>del>mod>wfx"); double lo = 1, hi = 0; float ol, orr;
          for (int i = 0; i < 96000; ++i) { const float x = 0.5f * std::sin(0.05f * float(i)); e.tick(g, x, x, x, x, ol, orr); if (i > 2000) { const double a = std::fabs(ol); lo = std::min(lo, a); hi = std::max(hi, a); } }
          CHECK(hi > 0.4 && lo < 0.1); }                                         // tremolo through the engine: the level really swings
        // all effects on together: stable, and the Level knob scales only the wet part
        { Engine e; e.prepare(48000); e.revOn = e.delOn = e.ifxOn = e.wfxOn = false; e.modOn = true; for (int k = 0; k < kModFx; ++k) { e.modUse[k] = true; e.mps[k].type = k; e.mps[k].speed = 1.3f; e.mps[k].depth = 0.7f; e.mps[k].regen = 0.3f; }
          const Graph g = parseGraph("ifx>rev>del>mod>wfx"); double pk = 0; bool fin = true; float ol, orr;
          for (int i = 0; i < 192000; ++i) { const float x = 0.4f * std::sin(0.04f * float(i)) + 0.2f * std::sin(0.27f * float(i)); e.tick(g, x, x * 0.9f, x, x, ol, orr); fin = fin && std::isfinite(ol) && std::isfinite(orr); pk = std::max(pk, (double)std::fabs(ol)); }
          CHECK(fin && pk < 4.0); std::printf("  all 8 together: peak %.2f\n", pk); }
        { double lv[2]; for (int w = 0; w < 2; ++w) { Engine e; e.prepare(48000); e.revOn = e.delOn = e.ifxOn = e.wfxOn = false; e.modOn = true; e.modMix = 1; e.modLevel = w ? 2.0f : 1.0f; e.modUse[0] = false; e.modUse[3] = true; e.mps[3].type = 3; e.mps[3].depth = 1; e.mps[3].speed = 3;
              const Graph g = parseGraph("ifx>rev>del>mod>wfx"); double en = 0; float ol, orr; for (int i = 0; i < 48000; ++i) { const float x = 0.3f * std::sin(0.05f * float(i)); e.tick(g, x, x, x, x, ol, orr); en += ol * ol; } lv[w] = en; }
          CHECK(lv[1] > lv[0] * 1.5); }
        // reverb and delay level
        { wm::engine::ReverbUnit r; r.prepare(48000); double e1 = 0, e2 = 0; for (int w = 0; w < 2; ++w) { r.reset(); wm::engine::ReverbSettings s; s.mix = 1; s.level = w ? 2.0f : 1.0f; s.shimmer = false; double en = 0; float l, rr; for (int i = 0; i < 48000; ++i) { const float x = i < 100 ? 0.5f : 0; r.process(x, x, l, rr, s); en += l * l; } (w ? e2 : e1) = en; } CHECK(e2 > e1 * 3.0); }
        { wm::engine::ShimmerDelay d; d.prepare(48000); double e1 = 0, e2 = 0; for (int w = 0; w < 2; ++w) { d.reset(); wm::engine::ShimmerDelayParams p; p.mix = 1; p.level = w ? 2.0f : 1.0f; p.shimmerAmount = 0; double en = 0; float l, rr; for (int i = 0; i < 48000; ++i) { const float x = i < 100 ? 0.5f : 0; d.process(x, x, l, rr, p); en += l * l; } (w ? e2 : e1) = en; } CHECK(e2 > e1 * 3.0); }
        std::printf("modulation ok\n");
    }

    // master limiter: on holds a loud signal under the Out ceiling, a quiet one passes untouched, In gain pushes into it
    { double pk1 = 0, pk2 = 0, dq = 0, dq2 = 0;
      MasterLimiter m; m.prepare(48000);
      for (int i = 0; i < 48000; ++i) { float l = 3.f * std::sin(0.07f * float(i)), r = l; m.process(l, r, 0.f, -1.f); if (i > 2000) pk1 = std::max(pk1, (double) std::fabs(l)); }
      MasterLimiter m2; m2.prepare(48000);
      for (int i = 0; i < 48000; ++i) { float l = 0.2f * std::sin(0.07f * float(i)), r = l; const float x = l; m2.process(l, r, 12.f, -3.f); if (i > 2000) { pk2 = std::max(pk2, (double) std::fabs(l)); } (void) x; }
      MasterLimiter m3; m3.prepare(48000);
      for (int i = 0; i < 48000; ++i) { float l = 0.3f * std::sin(0.05f * float(i)), r = l; const float x = l; m3.process(l, r, 0.f, -1.f); if (i > 100) dq = std::max(dq, (double) std::fabs(l - x)); }
      CHECK(pk1 <= 0.8913 + 1e-3 && dq < 1e-4); CHECK(pk2 <= 0.7080 + 1e-3 && pk2 > 0.6);
      std::printf("  master limiter: loud peak %.3f (ceiling 0.891), in +12 out -3 peak %.3f (ceiling 0.708), quiet error %.6f\n", pk1, pk2, dq); (void) dq2; }

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
