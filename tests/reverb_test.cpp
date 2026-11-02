#include <cstdio>
#include <cmath>
#include <cstdlib>
#include "../src/Reverb.h"
using namespace wm::engine;
static int fails = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL line %d: %s\n", __LINE__, #c); ++fails; } } while (0)
static float goertzel(const std::vector<float>& x, float f, float sr) {
    float w = 2 * 3.14159265f * f / sr, c = 2 * std::cos(w), s1 = 0, s2 = 0;
    for (float v : x) { float s = v + c * s1 - s2; s2 = s1; s1 = s; }
    return std::sqrt(s1 * s1 + s2 * s2 - c * s1 * s2) / float(x.size());
}
// Strongest frequency (Hz) between lo and hi, scanning in 5 Hz steps.
static float peakFreq(const std::vector<float>& x, float lo, float hi, float sr) {
    float best = 0, bf = 0;
    for (float f = lo; f <= hi; f += 5) { float a = goertzel(x, f, sr); if (a > best) { best = a; bf = f; } }
    return bf;
}
static float energy(const std::vector<float>& v, size_t a, size_t b) { float e = 0; for (size_t i = a; i < b && i < v.size(); ++i) e += v[i] * v[i]; return e; }

int main() {
    const float sr = 48000;
    // 1. Pitch shifter: 440 Hz in, ratio 2 -> energy at 880 Hz dominates 440 Hz
    {
        PitchShifter ps; ps.prepare(sr);
        std::vector<float> out;
        for (int i = 0; i < 48000; ++i) { float y = ps.tick(std::sin(2 * 3.14159265f * 440.0f * float(i) / sr), 2.0f); if (i > 9600) out.push_back(y); }
        float pf = peakFreq(out, 200, 2000, sr);
        CHECK(pf > 840 && pf < 920);                          // an octave up (the two-head method adds some warble)
        CHECK(goertzel(out, 440, sr) < 0.05f * goertzel(out, pf, sr));   // original pitch is gone
    }
    // 2. Reverbs: impulse response decays, no NaN, tail is long, higher decay setting = longer tail
    for (int type = 0; type < 3; ++type) {
        for (int pass = 0; pass < 2; ++pass) {
            ReverbUnit r; r.prepare(sr);
            ReverbSettings s; s.type = type == 2 ? ReverbType::Velvet : type ? ReverbType::Hall : ReverbType::Plate; s.mix = 1.0f; s.p.decay = pass ? 0.9f : 0.3f;
            std::vector<float> L(96000), R(96000);
            for (size_t i = 0; i < L.size(); ++i) r.process(i == 0 ? 1.0f : 0.0f, i == 0 ? 1.0f : 0.0f, L[i], R[i], s);
            bool finite = true; for (float v : L) finite = finite && std::isfinite(v);
            CHECK(finite);
            float e1 = energy(L, 2000, 12000), e2 = energy(L, 40000, 50000), e3 = energy(L, 86000, 96000);
            CHECK(e1 > 1e-6f);
            CHECK(e2 < e1 && e3 < e2);
            static float tails[3][2];
            tails[type][pass] = e2;
            if (pass == 1) CHECK(tails[type][1] > tails[type][0]);
            CHECK(std::fabs(energy(L, 0, 96000) - energy(R, 0, 96000)) / energy(L, 0, 96000) < 0.9f);   // both channels alive
        }
    }
    // 3. Stability: loud noise through all four types, then silence: output must settle and stay bounded
    for (int t8 = 0; t8 < 15; ++t8) {
        const int t = t8 % 5; const float blend = t8 < 5 ? 0.0f : t8 < 10 ? 0.5f : 1.0f;
        ReverbUnit r; r.prepare(sr);
        static const int ty[5] = {0, 1, 0, 1, 2}; ReverbSettings s; s.type = ReverbType(ty[t]); s.shimmer = (t == 2 || t == 3); s.lexBlend = blend; s.mix = 1.0f; s.p.decay = 0.95f; s.shimmerAmount = 0.9f; s.shimmer2Amount = 1.0f; s.shimmer2Semis = 19;
        float peak = 0, l, rr; std::srand(1);
        for (int i = 0; i < 96000; ++i) { float n = (std::rand() / float(RAND_MAX) - 0.5f) * 2; r.process(n, n, l, rr, s); peak = std::max(peak, std::fabs(l)); }
        CHECK(std::isfinite(peak) && peak < 20.0f);
        float endPeak = 0; for (int i = 0; i < 480000; ++i) { r.process(0, 0, l, rr, s); if (i > 470000) endPeak = std::max(endPeak, std::fabs(l)); }
        CHECK(std::isfinite(endPeak) && endPeak < 1.0f); if (!(endPeak < 1.0f)) std::printf("type %d endPeak %g\n", t, endPeak);
    }
    // 4. Shimmer delay: echo arrives at the set time, shifted up an octave on the second repeat
    {
        ShimmerDelay d; d.prepare(sr); ShimmerDelayParams p; p.timeMs = 250; p.mix = 1.0f; p.feedback = 0.7f; p.shiftSemis = 12;
        std::vector<float> out;
        for (int i = 0; i < 96000; ++i) { float l, r2; float x = i < 4800 ? std::sin(2 * 3.14159265f * 440.0f * float(i) / sr) : 0.0f; d.process(x, x, l, r2, p); out.push_back(l); }
        CHECK(energy(out, 12000 + 200, 12000 + 4600) > 0.1f);              // first echo near 250 ms
        CHECK(energy(out, 6000, 11000) < 0.001f);                            // quiet before it
        std::vector<float> second(out.begin() + 24000 + 3000, out.begin() + 24000 + 4800);   // second echo window
        { float pf2 = peakFreq(second, 300, 1500, sr); CHECK(pf2 > 800 && pf2 < 950); }
    }
    // 5. Velvet colour: bright setting has more high-frequency content in the tail than dark
    {
        float hf[2];
        for (int k = 0; k < 2; ++k) {
            ReverbUnit r; r.prepare(sr); ReverbSettings s; s.type = ReverbType::Velvet; s.mix = 1.0f; s.p.color = k ? 1.0f : -1.0f;
            float l, rr, prev = 0, d = 0, tot = 0; std::srand(3);
            for (int i = 0; i < 48000; ++i) { float n = i < 2400 ? (std::rand() / float(RAND_MAX) - 0.5f) : 0.0f; r.process(n, n, l, rr, s);
                if (i > 6000) { d += std::fabs(l - prev); tot += std::fabs(l); } prev = l; }
            hf[k] = d / (tot + 1e-9f);
        }
        CHECK(hf[1] > hf[0] * 1.3f);
    }
    // 6. Wet filters: low-pass at 1 kHz leaves less high-frequency content than open; high-pass removes low end
    {
        float hf[2], lo[2];
        for (int k = 0; k < 2; ++k) {
            ReverbUnit r; r.prepare(sr); ReverbSettings s; s.type = ReverbType::Hall; s.mix = 1.0f;
            if (k) { s.wetLpHz = 1000.0f; }
            float l, rr, prev = 0, d = 0, tot = 0; std::srand(5);
            for (int i = 0; i < 48000; ++i) { float n = i < 2400 ? (std::rand() / float(RAND_MAX) - 0.5f) : 0.0f; r.process(n, n, l, rr, s);
                if (i > 6000) { d += std::fabs(l - prev); tot += std::fabs(l); } prev = l; }
            hf[k] = d / (tot + 1e-9f);
        }
        CHECK(hf[1] < hf[0] * 0.6f);
        for (int k = 0; k < 2; ++k) {
            ReverbUnit r; r.prepare(sr); ReverbSettings s; s.type = ReverbType::Hall; s.mix = 1.0f;
            if (k) { s.wetHpHz = 1500.0f; }
            float l, rr, e = 0, e2 = 0; std::srand(5);
            for (int i = 0; i < 48000; ++i) { float n = i < 2400 ? (std::rand() / float(RAND_MAX) - 0.5f) : 0.0f; r.process(n, n, l, rr, s); if (i > 6000) { e += l * l; } }
            (void)e2; lo[k] = e;
        }
        CHECK(lo[1] < lo[0] * 0.6f);
    }
    // 7. Second shimmer voice adds energy at its own pitch: with amount 0 it is exactly the same as before
    {
        ReverbUnit a, b; a.prepare(sr); b.prepare(sr);
        ReverbSettings s; s.type = ReverbType::Hall; s.shimmer = true; s.mix = 1.0f; s.shimmer2Semis = -12; s.shimmer2Amount = 0.0f;
        ReverbSettings s2 = s; s2.shimmer2Amount = 1.0f;
        float la, lb, rr, diff = 0;
        for (int i = 0; i < 48000; ++i) { float x = i < 2400 ? std::sin(2*3.14159265f*440.0f*float(i)/sr) : 0.0f; a.process(x, x, la, rr, s); b.process(x, x, lb, rr, s2); if (i > 20000) diff += std::fabs(la - lb); }
        CHECK(diff > 0.01f);
    }

    // 8. Lexicat (Lexicon-style module, lexBlend = 1): Mid RT sets the decay, Bass Multiply lengthens only the lows, Shape delays the build-up
    {
        auto ir = [&](float blend, ReverbType t, auto tweak) {
            ReverbUnit r; r.prepare(sr); ReverbSettings s; s.type = t; s.lexBlend = blend; s.mix = 1.0f; s.p.preDelayMs = 0; s.p.early = 0; tweak(s);
            std::vector<float> L(4 * 96000); float rr;
            for (size_t i = 0; i < L.size(); ++i) r.process(i == 0 ? 1.0f : 0.0f, i == 0 ? 1.0f : 0.0f, L[i], rr, s);
            return L;
        };
        auto band = [&](const std::vector<float>& x, bool low, size_t a2, size_t b2) {
            float lp = 0, k = 1.0f - std::exp(-6.2831853f * 300.0f / sr), e = 0;
            for (size_t i = 0; i < b2 && i < x.size(); ++i) { lp += k * (x[i] - lp); if (i >= a2) { float v = low ? lp : x[i] - lp; e += v * v; } }
            return e;
        };
        for (int mv = 0; mv < 2; ++mv) {
            auto a = ir(1.0f, ReverbType::Hall, [&](ReverbSettings& s) { s.p.decay = 0.4f; s.p.lexMode = mv; });
            auto b = ir(1.0f, ReverbType::Hall, [&](ReverbSettings& s) { s.p.decay = 0.8f; s.p.lexMode = mv; });
            bool fin = true; for (float v : a) fin = fin && std::isfinite(v);
            CHECK(fin);
            CHECK(energy(a, 20000, 30000) > 1e-8f);
            CHECK(energy(b, 100000, 120000) > 4.0f * energy(a, 100000, 120000));
            CHECK(energy(b, 240000, 260000) < energy(b, 100000, 120000));
            auto n1 = ir(1.0f, ReverbType::Hall, [&](ReverbSettings& s) { s.p.decay = 0.6f; s.p.bass = 1.0f; s.p.lexMode = mv; });
            auto n2 = ir(1.0f, ReverbType::Hall, [&](ReverbSettings& s) { s.p.decay = 0.6f; s.p.bass = 2.0f; s.p.lexMode = mv; });
            float lowGain = band(n2, true, 120000, 160000) / (band(n1, true, 120000, 160000) + 1e-12f);
            float hiGain = band(n2, false, 120000, 160000) / (band(n1, false, 120000, 160000) + 1e-12f);
            CHECK(lowGain > 2.0f);
            CHECK(hiGain < 0.5f * lowGain);
            auto fast = ir(1.0f, ReverbType::Hall, [&](ReverbSettings& s) { s.p.shape = 0.0f; s.p.lexMode = mv; });
            auto slow = ir(1.0f, ReverbType::Hall, [&](ReverbSettings& s) { s.p.shape = 1.0f; s.p.spread = 1.0f; s.p.lexMode = mv; });
            CHECK(energy(slow, 1400, 7200) < 0.6f * energy(fast, 1400, 7200));
            auto e0 = ir(1.0f, ReverbType::Hall, [&](ReverbSettings& s) { s.p.early = 0.0f; s.p.lexMode = mv; });
            auto e1 = ir(1.0f, ReverbType::Hall, [&](ReverbSettings& s) { s.p.early = 1.0f; s.p.lexMode = mv; });
            CHECK(energy(e1, 0, 2880) > energy(e0, 0, 2880));
        }
        // blend 0 is the plain reverb, exactly; blend 0.5 differs from both ends
        auto m0 = ir(0.0f, ReverbType::Hall, [](ReverbSettings&) {}), m5 = ir(0.5f, ReverbType::Hall, [](ReverbSettings&) {}), m1 = ir(1.0f, ReverbType::Hall, [](ReverbSettings&) {});
        ReverbUnit plain; plain.prepare(sr); ReverbSettings sp; sp.type = ReverbType::Hall; sp.mix = 1.0f; sp.p.preDelayMs = 0; sp.p.early = 0; float pl, pr, dsum = 0;
        for (size_t i = 0; i < 20000; ++i) { plain.process(i == 0 ? 1.0f : 0.0f, i == 0 ? 1.0f : 0.0f, pl, pr, sp); dsum += std::fabs(pl - m0[i]); }
        CHECK(dsum == 0.0f);
        float d05 = 0, d15 = 0; for (size_t i = 0; i < 20000; ++i) { d05 += std::fabs(m5[i] - m0[i]); d15 += std::fabs(m5[i] - m1[i]); }
        CHECK(d05 > 0.01f && d15 > 0.01f);
        // level match: same Decay, the Lexicat tail has about the same energy as Hall (within 3 dB)
        float eh = energy(m0, 4000, 80000), el = energy(m1, 4000, 80000);
        std::printf("level Lexicat vs Hall: %.1f dB\n", 10.0f * std::log10(el / eh));
        CHECK(std::fabs(10.0f * std::log10(el / eh)) < 3.0f);
        // smoothness of the late tail spectrum (less metallic = smaller ripple in dB), 1.0 to 1.5 s, 200 Hz to 8 kHz
        auto ripple = [&](const std::vector<float>& x) {
            const size_t a2 = 48000, n = 24000; std::vector<float> w(n); for (size_t i = 0; i < n; ++i) w[i] = x[a2 + i] * (0.5f - 0.5f * std::cos(6.2831853f * float(i) / float(n)));
            std::vector<float> db; for (int k = 0; k < 160; ++k) { float f = 200.0f * std::pow(40.0f, float(k) / 159.0f); db.push_back(20.0f * std::log10(goertzel(w, f, sr) + 1e-12f)); }
            float m = 0; for (float v : db) m += v; m /= float(db.size()); float v2 = 0; for (float v : db) v2 += (v - m) * (v - m); return std::sqrt(v2 / float(db.size()));
        };
        auto hallIr = ir(0.0f, ReverbType::Hall, [](ReverbSettings& s) { s.p.decay = 0.7f; s.p.modDepth = 0.0f; });
        auto lexStill = ir(1.0f, ReverbType::Hall, [](ReverbSettings& s) { s.p.decay = 0.7f; s.p.wander = 0.0f; });
        auto lexMove = ir(1.0f, ReverbType::Hall, [](ReverbSettings& s) { s.p.decay = 0.7f; s.p.wander = 1.0f; s.p.lexMode = 1; });
        std::printf("tail ripple dB: Hall %.2f, Lexicat still %.2f, Lexicat moving %.2f\n", ripple(hallIr), ripple(lexStill), ripple(lexMove));
        CHECK(ripple(lexStill) < ripple(hallIr) && ripple(lexMove) < ripple(hallIr));   // Lexicat tail is smoother (less metallic) than Hall
    }
    // 9. Delay extras off = same output as the plain delay; cross feedback moves echoes to the other channel and stays bounded
    {
        ShimmerDelay a, b; a.prepare(sr); b.prepare(sr); ShimmerDelayParams p0, p1; p0.mix = p1.mix = 1.0f; p1.crossFb = 0.0f; p1.wide = 0.0f; p1.tap2 = 0.0f;
        float la, lb, ra, rb, diff = 0;
        for (int i = 0; i < 48000; ++i) { float x = i < 2400 ? std::sin(0.05f * float(i)) : 0.0f; a.process(x, x, la, ra, p0); b.process(x, x, lb, rb, p1); diff += std::fabs(la - lb) + std::fabs(ra - rb); }
        CHECK(diff == 0.0f);
        ShimmerDelay c; c.prepare(sr); ShimmerDelayParams pc; pc.mix = 1.0f; pc.timeMs = 100; pc.feedback = 0.9f; pc.shimmerAmount = 0.0f; pc.crossFb = 1.0f; pc.tap2 = 0.7f; pc.wide = 0.3f;
        float eL = 0, eR = 0, lc, rc, pk = 0;
        for (int i = 0; i < 96000; ++i) { float x = i == 0 ? 1.0f : 0.0f; c.process(x, 0.0f, lc, rc, pc); if (i > 2000) { eL += lc * lc; eR += rc * rc; } pk = std::max(pk, std::max(std::fabs(lc), std::fabs(rc))); }
        CHECK(std::isfinite(pk) && pk < 4.0f);
        CHECK(eR > 0.05f * eL);                                                       // the echo reaches the right channel although only the left was fed
    }
    // 10. New in this rework: shimmer is independent of the type; Lexicat modes differ and stay stable; delay voices
    {
        auto run = [&](ReverbType t, bool shim, int mode, float blend, float& peak) {
            ReverbUnit r; r.prepare(sr); ReverbSettings s; s.type = t; s.shimmer = shim; s.lexBlend = blend; s.p.lexMode = mode; s.mix = 1.0f; s.p.decay = 0.9f; s.shimmerAmount = 0.6f;
            float l, rr, e = 0; peak = 0;
            for (int i = 0; i < 96000; ++i) { float x = i < 2400 ? std::sin(0.07f * float(i)) : 0.0f; r.process(x, x, l, rr, s); peak = std::max(peak, std::fabs(l)); if (i > 20000) e += l * l; if (!std::isfinite(l)) peak = 1e9f; }
            return e;
        };
        float pk;
        for (int t = 0; t < 3; ++t) {
            float pOff, pOn; const float eOff = run(ReverbType(t), false, 0, 0.f, pOff), eOn = run(ReverbType(t), true, 0, 0.f, pOn);
            CHECK(std::fabs(eOn - eOff) > 1e-4f * (eOff + 1e-9f));           // shimmer changes the sound for every type
            CHECK(pOn < 4.0f && pOff < 4.0f);
        }
        float eM[5]; for (int m = 0; m < 5; ++m) { eM[m] = run(ReverbType::Hall, true, m, 1.0f, pk); CHECK(pk < 4.0f); }
        int distinct = 0; for (int m = 1; m < 5; ++m) if (std::fabs(eM[m] - eM[0]) > 0.01f * eM[0]) ++distinct;
        CHECK(distinct == 4);                                                   // each mode sounds different from Hall
        std::printf("lexicat modes energy: %.3f %.3f %.3f %.3f %.3f\n", eM[0], eM[1], eM[2], eM[3], eM[4]);
        ShimmerDelay a, b; a.prepare(sr); b.prepare(sr); ShimmerDelayParams p0, p1; p0.mix = p1.mix = 1.0f; p1.voices = 6; p1.slope = 0.5f; p1.wander = 0.8f; p1.spin = 0.6f; p1.diffusion = 0.7f; p1.feedback = 0.8f;
        float la, lb, ra, rb, diff = 0, pk2 = 0;
        for (int i = 0; i < 192000; ++i) { float x = i < 2400 ? std::sin(0.05f * float(i)) : 0.0f; a.process(x, x, la, ra, p0); b.process(x, x, lb, rb, p1); diff += std::fabs(la - lb); pk2 = std::max(pk2, std::max(std::fabs(lb), std::fabs(rb))); }
        CHECK(diff > 1.0f && std::isfinite(pk2) && pk2 < 4.0f);                 // voices change the echo and stay bounded
    }
    // 11. New reverb types: stable, level matched to Hall, special control does something, gate cuts, reverse swells
    {
        auto run = [&](ReverbType t, float special, float& peak, float& late) {
            ReverbUnit r; r.prepare(sr); ReverbSettings s; s.type = t; s.shimmer = false; s.mix = 1.0f; s.p.decay = 0.6f; s.p.special = special;
            float l, rr, e = 0; peak = 0; late = 0;
            for (int i = 0; i < 48000 * 6; ++i) { float x = i < 2400 ? std::sin(0.07f * float(i)) * 0.5f : 0.0f; r.process(x, x, l, rr, s); if (!std::isfinite(l) || !std::isfinite(rr)) { peak = 1e9f; break; } peak = std::max(peak, std::fabs(l)); e += l * l; if (i > 48000 * 2) late += l * l; }
            return e;
        };
        float pk, lt; const float eHall = run(ReverbType::Hall, 0.5f, pk, lt);
        for (int t = 3; t < 9; ++t) {
            const float e = run(ReverbType(t), 0.5f, pk, lt);
            CHECK(pk < 4.0f && e > 0.0f);
            CHECK(std::fabs(10.0f * std::log10(e / eHall)) < 5.0f);              // within 5 dB of Hall
        }
        {   // Spring: boing changes the waveform (allpass chirp keeps the energy, so compare samples)
            auto wave = [&](float special) { ReverbUnit r; r.prepare(sr); ReverbSettings s; s.type = ReverbType::Spring; s.shimmer = false; s.mix = 1.0f; s.p.special = special; std::vector<float> o; float l, rr;
                for (int i = 0; i < 24000; ++i) { float x = i < 100 ? std::sin(0.3f * float(i)) : 0.0f; r.process(x, x, l, rr, s); o.push_back(l); } return o; };
            const auto w1 = wave(0.1f), w2 = wave(0.9f); float dd = 0, ee = 0; for (size_t i = 0; i < w1.size(); ++i) { dd += std::fabs(w1[i] - w2[i]); ee += std::fabs(w1[i]); }
            CHECK(dd > 0.2f * ee);
        }
        float gl1, gl2; run(ReverbType::Gated, 0.0f, pk, gl1); run(ReverbType::Gated, 1.0f, pk, gl2);
        CHECK(gl1 < 1e-6f);                                                      // hold 40 ms: nothing left after 2 s
        float rl; const float eRev = run(ReverbType::Reverse, 0.5f, pk, rl), eNorm = run(ReverbType::Hall, 0.5f, pk, lt); CHECK(std::fabs(eRev - eNorm) > 0.01f * eNorm);
        std::printf("new reverb types ok, Hall %.2f\n", eHall);
    }
    // 12. Delay types: Studio default unchanged, others finite and different, Hold keeps repeats, ducker dips, Pong alternates sides
    {
        auto runD = [&](ShimmerDelayParams p, int n, std::vector<float>* L, std::vector<float>* R) {
            ShimmerDelay d; d.prepare(sr); float l, r, pk = 0;
            for (int i = 0; i < n; ++i) { float x = i < 200 ? std::sin(0.05f * float(i)) : 0.0f; d.process(x, x, l, r, p); if (!std::isfinite(l) || !std::isfinite(r)) return 1e9f; pk = std::max(pk, std::max(std::fabs(l), std::fabs(r))); if (L) { L->push_back(l); R->push_back(r); } }
            return pk;
        };
        ShimmerDelayParams b; b.mix = 1.0f; b.timeMs = 200; b.feedback = 0.6f; b.shimmerAmount = 0.0f;
        std::vector<float> base, baseR; runD(b, 96000, &base, &baseR);
        for (int t = 1; t < 5; ++t) { ShimmerDelayParams q = b; q.type = t; q.modDepth = 0.6f; std::vector<float> a, ar; const float pk = runD(q, 96000, &a, &ar); CHECK(pk < 4.0f && pk > 0.01f); float dd = 0; for (size_t i = 0; i < a.size(); ++i) dd += std::fabs(a[i] - base[i]); CHECK(dd > 1.0f); }
        { ShimmerDelayParams q = b; q.type = 2; std::vector<float> a, ar; runD(q, 96000, &a, &ar);
          const float e1l = energy(a, 9000, 11000), e1r = energy(ar, 9000, 11000), e2l = energy(a, 19000, 21000), e2r = energy(ar, 19000, 21000);
          CHECK(e1l > 10.0f * e1r && e2r > 10.0f * e2l); }                      // first echo left, second echo right
        {   // Hold: after the input has gone in, the repeats keep looping
            auto tailE = [&](bool useHold) { ShimmerDelay d; d.prepare(sr); ShimmerDelayParams q = b; q.dampHz = 3000; float l, r, e = 0;
                for (int i = 0; i < 48000 * 6; ++i) { float x = i < 200 ? std::sin(0.05f * float(i)) : 0.0f; q.hold = useHold && i > 3000; d.process(x, x, l, r, q); if (i > 48000 * 5) e += l * l; } return e; };
            CHECK(tailE(false) < 1e-4f && tailE(true) > 0.001f);
        }
        { ShimmerDelay d; d.prepare(sr); ShimmerDelayParams q = b; q.duck = 1.0f; q.duckThr = 0.2f; ShimmerDelay e2; e2.prepare(sr); ShimmerDelayParams q0 = b; float l, r, la, ra, eD = 0, eN = 0;
          for (int i = 0; i < 48000; ++i) { float x = 0.5f * std::sin(0.05f * float(i)); d.process(x, x, l, r, q); e2.process(x, x, la, ra, q0); if (i > 20000) { eD += l * l; eN += la * la; } }
          CHECK(eD < 0.25f * eN); }                                              // ducker dips the repeats while the input plays
        std::printf("delay types ok\n");
    }
    if (!fails) std::puts("reverb_test: all passed");
    return fails ? 1 : 0;
}
