// Measurements (not pass/fail unit tests): level, tail length, width, DC, stability, shimmer purity and aliasing.
// Build and run: sh tests/measure.sh   (prints a table; numbers are MEASURED on this code, not heard)
#include <cstdio>
#include <cmath>
#include <cstdlib>
#include <vector>
#include <string>
#include "../src/Reverb.h"
#include "../src/Fx.h"
using namespace wm::engine;
static const float SR = 48000.f, PI = 3.14159265f;
static float rms(const std::vector<float>& v, size_t a, size_t b) { double e = 0; for (size_t i = a; i < b && i < v.size(); ++i) e += double(v[i]) * v[i]; return float(std::sqrt(e / std::max<size_t>(1, std::min(b, v.size()) - a))); }
static float db(float x) { return 20.f * std::log10(x + 1e-12f); }
static float amp(const std::vector<float>& x, float f, size_t a, size_t b) {   // single bin DFT amplitude
    double re = 0, im = 0, w = 2 * PI * f / SR; for (size_t i = a; i < b; ++i) { re += x[i] * std::cos(w * double(i)); im += x[i] * std::sin(w * double(i)); }
    return float(2.0 * std::sqrt(re * re + im * im) / double(b - a)); }
static unsigned rs = 12345; static float rnd() { rs = rs * 1664525u + 1013904223u; return float(rs >> 8) / 8388608.f - 1.f; }
// pink-ish noise (3 pole filter, Paul Kellet)
struct Pink { float b0 = 0, b1 = 0, b2 = 0; float tick() { float w = rnd(); b0 = 0.99765f * b0 + w * 0.0990460f; b1 = 0.96300f * b1 + w * 0.2965164f; b2 = 0.57000f * b2 + w * 1.0526913f; return (b0 + b1 + b2 + w * 0.1848f) * 0.22f; } };
static const char* tn[5] = {"Plate", "Hall", "ShimmerPlate", "ShimmerHall", "Velvet"};
static ReverbSettings mk(int t) { ReverbSettings s; s.type = ReverbType(t); s.mix = 1.f; return s; }

int main() {
    std::printf("== 1. Reverb, default settings, wet only (mix 100 %%) ==\n");
    std::printf("%-13s %9s %9s %8s %8s %9s %8s\n", "type", "gain dB", "RT60 s", "corr LR", "DC", "peak", "NaN");
    for (int t = 0; t < 5; ++t) {
        ReverbUnit r; r.prepare(SR); ReverbSettings s = mk(t); Pink pk;
        std::vector<float> L, R, in; bool fin = true; float peak = 0;
        for (int i = 0; i < int(SR * 6); ++i) { float x = pk.tick() * 0.25f, a, b; in.push_back(x); r.process(x, x, a, b, s); L.push_back(a); R.push_back(b); fin = fin && std::isfinite(a) && std::isfinite(b); peak = std::max(peak, std::fabs(a)); }
        double sl = 0, sr2 = 0, sx = 0; for (size_t i = size_t(SR * 2); i < L.size(); ++i) { sl += L[i] * L[i]; sr2 += R[i] * R[i]; sx += L[i] * R[i]; }
        double dc = 0; for (size_t i = size_t(SR * 2); i < L.size(); ++i) dc += L[i]; dc /= double(L.size() - size_t(SR * 2));
        // RT60 from impulse response, Schroeder backward integration, T20 extrapolated x3
        ReverbUnit q; q.prepare(SR); std::vector<float> ir; for (int i = 0; i < int(SR * 12); ++i) { float a, b; q.process(i == 0 ? 1.f : 0.f, i == 0 ? 1.f : 0.f, a, b, s); ir.push_back(a); }
        std::vector<double> e(ir.size()); double acc = 0; for (size_t i = ir.size(); i-- > 0;) { acc += double(ir[i]) * ir[i]; e[i] = acc; }
        double t5 = -1, t25 = -1; for (size_t i = 0; i < e.size(); ++i) { double d = 10 * std::log10(e[i] / e[0] + 1e-30); if (t5 < 0 && d < -5) t5 = double(i) / SR; if (t25 < 0 && d < -25) { t25 = double(i) / SR; break; } }
        std::printf("%-13s %9.2f %9.2f %8.2f %8.5f %9.3f %8s\n", tn[t], db(rms(L, size_t(SR * 2), L.size()) / rms(in, size_t(SR * 2), in.size())), t25 > 0 ? (t25 - t5) * 3.0 : -1.0, float(sx / std::sqrt(sl * sr2 + 1e-30)), dc, peak, fin ? "ok" : "BAD");
    }
    std::printf("\n== 2. Decay knob sweep (Hall, impulse): RT60 s for decay 0.1 .. 1.0 ==\n");
    for (float d : {0.1f, 0.3f, 0.5f, 0.7f, 0.9f, 1.0f}) {
        ReverbUnit q; q.prepare(SR); ReverbSettings s = mk(1); s.p.decay = d; std::vector<float> ir; for (int i = 0; i < int(SR * 30); ++i) { float a, b; q.process(i == 0 ? 1.f : 0.f, i == 0 ? 1.f : 0.f, a, b, s); ir.push_back(a); }
        std::vector<double> e(ir.size()); double acc = 0; for (size_t i = ir.size(); i-- > 0;) { acc += double(ir[i]) * ir[i]; e[i] = acc; }
        double t5 = -1, t25 = -1; for (size_t i = 0; i < e.size(); ++i) { double dd = 10 * std::log10(e[i] / e[0] + 1e-30); if (t5 < 0 && dd < -5) t5 = double(i) / SR; if (t25 < 0 && dd < -25) { t25 = double(i) / SR; break; } }
        std::printf("  decay %.1f -> RT60 %.2f s%s\n", d, t25 > 0 ? (t25 - t5) * 3.0 : -1.0, t25 < 0 ? " (longer than 30 s)" : "");
    }
    std::printf("\n== 3. Worst-case stability: max decay, max shimmer, both voices, 40 s loud noise, then silence ==\n");
    for (int t : {2, 3}) {
        ReverbUnit r; r.prepare(SR); ReverbSettings s = mk(t); s.p.decay = 1.f; s.shimmerAmount = 1.f; s.shimmer2Amount = 1.f; Pink pk; float peak = 0; bool fin = true;
        for (int i = 0; i < int(SR * 40); ++i) { float x = pk.tick() * 0.9f, a, b; r.process(x, x, a, b, s); peak = std::max(peak, std::max(std::fabs(a), std::fabs(b))); fin = fin && std::isfinite(a) && std::isfinite(b); }
        float tail = 0, t1 = 0; for (int i = 0; i < int(SR * 10); ++i) { float a, b; r.process(0, 0, a, b, s); if (i < int(SR)) t1 = std::max(t1, std::fabs(a)); if (i >= int(SR * 9)) tail = std::max(tail, std::fabs(a)); }
        std::printf("  %-13s peak %.2f, finite %s, peak in 1st s of silence %.3f, in 10th s %.3f\n", tn[t], peak, fin ? "yes" : "NO", t1, tail);
    }
    std::printf("\n== 4. Shimmer: 440 Hz sine in, wet only, last 2 s. Level in 60 Hz wide bands around each octave (dB, relative to the 440 band) ==\n");
    for (int t : {2, 3}) {
        ReverbUnit r; r.prepare(SR); ReverbSettings s = mk(t); std::vector<float> o; for (int i = 0; i < int(SR * 8); ++i) { float x = 0.3f * std::sin(2 * PI * 440.f * float(i) / SR), a, b; r.process(x, x, a, b, s); o.push_back(a); }
        size_t a0 = size_t(SR * 6), a1 = o.size(); auto band = [&](float c) { double e = 0; for (float f = c - 30; f <= c + 30; f += 3) { float A = amp(o, f, a0, a1); e += 0.5 * A * A; } return float(std::sqrt(e)); };
        float b0 = band(440); std::printf("  %-13s 220 Hz %.1f, 440 Hz %.1f (abs %.1f), 880 Hz %.1f, 1760 Hz %.1f, 3520 Hz %.1f\n", tn[t], db(band(220) / b0), 0.f, db(b0), db(band(880) / b0), db(band(1760) / b0), db(band(3520) / b0));
    }
    std::printf("\n== 5. PitchShifter alone (ratio 2), 1 kHz sine: spurious lines relative to 2 kHz ==\n");
    { PitchShifter ps; ps.prepare(SR); std::vector<float> o; for (int i = 0; i < int(SR * 4); ++i) o.push_back(ps.tick(0.5f * std::sin(2 * PI * 1000.f * float(i) / SR), 2.f));
      size_t a0 = size_t(SR), a1 = o.size(); float m = amp(o, 2000, a0, a1); std::printf("  2 kHz %.1f dB; 1 kHz %.1f dB rel; 4 kHz %.1f dB rel; 3 kHz %.1f dB rel; 1.5 kHz %.1f dB rel\n", db(m), db(amp(o, 1000, a0, a1) / m), db(amp(o, 4000, a0, a1) / m), db(amp(o, 3000, a0, a1) / m), db(amp(o, 1500, a0, a1) / m)); }
    std::printf("\n== 6. Shimmer delay, feedback up to 0.95, 20 s noise, then silence ==\n");
    for (float fb : {0.55f, 0.8f, 0.95f, 1.0f}) { ShimmerDelay d; d.prepare(SR); ShimmerDelayParams p; p.feedback = fb; p.mix = 1.f; Pink pk; float peak = 0; bool fin = true;
        for (int i = 0; i < int(SR * 20); ++i) { float x = pk.tick() * 0.5f, a, b; d.process(x, x, a, b, p); peak = std::max(peak, std::fabs(a)); fin = fin && std::isfinite(a); }
        float tail = 0; for (int i = 0; i < int(SR * 10); ++i) { float a, b; d.process(0, 0, a, b, p); if (i >= int(SR * 9)) tail = std::max(tail, std::fabs(a)); }
        std::printf("  feedback %.2f: peak %.2f, finite %s, peak in the 10th second after input stops %.3f\n", fb, peak, fin ? "yes" : "NO", tail); }
    std::printf("\n== 7. Saturation (ADAA) in the input FX: 10 kHz and 5 kHz sine, drive sweep, 100 %% sat. Alias lines vs fundamental ==\n");
    for (float f0 : {5000.f, 10000.f}) for (float dr : {6.f, 18.f, 30.f}) { wm::fx::InFx fx; fx.prepare(SR); wm::fx::InParams p; p.drive = dr; p.satMix = 1.f; std::vector<float> o;
        for (int i = 0; i < int(SR * 1); ++i) { float x = 0.5f * std::sin(2 * PI * f0 * float(i) / SR); fx.process(x, x, p); o.push_back(fx.oL); }
        float fund = amp(o, f0, 4800, o.size()); float worst = 0, wf = 0; for (float f = 100; f < 23900; f += 50) { float k = f0 > 0 ? std::fmod(f, f0) : 1; bool isHarm = false; for (int h = 1; h < 12; ++h) { float hf = h * f0; float fold = std::fabs(std::fmod(hf + SR / 2, SR) - SR / 2); if (std::fabs(f - fold) < 60) isHarm = true; }
            (void) k; if (std::fabs(f - f0) < 60) continue; float a = amp(o, f, 4800, o.size()); if (!isHarm && a > worst) { worst = a; wf = f; } }
        std::printf("  %5.0f Hz, drive %2.0f dB: fundamental %.1f dB, in/out level %+.1f dB, strongest non-harmonic line %.1f dB rel at %.0f Hz\n", f0, dr, db(fund), db(fund / 0.5f), db(worst / fund), wf); }
    std::printf("\n== 8. Wet FX chain: level change of squash and saturation, noise in ==\n");
    for (int mode = 0; mode < 3; ++mode) { wm::fx::WetFx w; w.prepare(SR); wm::fx::WetParams p; if (mode == 1) { p.satMix = 1; p.satDrive = 18; } if (mode == 2) { p.squashMix = 1; p.squashDepth = 1; } Pink pk; std::vector<float> in, o;
        for (int i = 0; i < int(SR * 4); ++i) { float x = pk.tick() * 0.25f; in.push_back(x); w.process(x, x, 0, 0, p); o.push_back(w.oL); }
        std::printf("  %-18s level %+.2f dB, peak %.2f\n", mode == 0 ? "bypass settings" : mode == 1 ? "sat 18 dB, 100%" : "squash 100%, depth 1", db(rms(o, 48000, o.size()) / rms(in, 48000, in.size())), *std::max_element(o.begin(), o.end())); }
    return 0; }
