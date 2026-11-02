// Input FX, wet FX (saturation, squash, width, duck, boost) and the routing graph engine of shimmerstuff.
// Pure C++, no JUCE, no allocation after construction. Ported from the design mockup (docs/mockup/shimmerstuff.html)
// so the plugin and the mockup behave the same. Original code; the squash and duck ideas are generic techniques.
#pragma once
#include "Reverb.h"
#include <array>
#include <cstdlib>
#include <string>

namespace wm::fx {

inline double lcosh(double x) { x = std::fabs(x); return x + std::log1p(std::exp(-2.0 * x)) - 0.6931471805599453; }

// First order antiderivative antialiased tanh: y = (F(x) - F(x1)) / (x - x1), F = ln cosh.
struct Adaa {
    double x1[2] = {0, 0}, F1[2] = {0, 0};
    double tick(int c, double x) {
        const double F = lcosh(x), dx = x - x1[c];
        const double y = std::fabs(dx) < 1e-5 ? std::tanh(0.5 * (x + x1[c])) : (F - F1[c]) / dx;
        x1[c] = x; F1[c] = F; return y;
    }
};
inline float satStage(Adaa& ad, int c, float x, float driveDb, float mix) {
    if (mix <= 0.0005f) { ad.x1[c] = x; ad.F1[c] = lcosh(x); return x; }
    const double g = std::pow(10.0, driveDb / 20.0), y = ad.tick(c, x * g) / std::sqrt(g);
    return float(x * (1.0 - mix) + y * mix);
}
struct OnePole { float z[2] = {0, 0}; float lp(int c, float x, float k) { z[c] += k * (x - z[c]); return z[c]; } };

struct InParams { float hp = 20, lp = 20000, drive = 6, satMix = 0; };
class InFx {
public:
    void prepare(float sr) { sr_ = sr; hp_ = OnePole(); lp_ = OnePole(); ad_ = Adaa(); }
    void process(float l, float r, const InParams& p) {
        const float kh = 1.f - std::exp(-6.2831853f * p.hp / sr_), kl = 1.f - std::exp(-6.2831853f * p.lp / sr_);
        float o[2] = {l, r};
        for (int c = 0; c < 2; ++c) {
            float x = o[c];
            if (p.hp > 21) x -= hp_.lp(c, x, kh); else hp_.z[c] = 0;
            if (p.lp < 19900) x = lp_.lp(c, x, kl); else lp_.z[c] = x;
            o[c] = satStage(ad_, c, x, p.drive, p.satMix);
        }
        oL = o[0]; oR = o[1];
    }
    float oL = 0, oR = 0;
private:
    float sr_ = 48000; OnePole hp_, lp_; Adaa ad_;
};

// Duck shapes over one cycle: 1 = fully ducked, 0 = open.
constexpr int kNumShapes = 8;
inline const char* shapeName(int i) { static const char* n[kNumShapes] = {"Pump", "Hard", "Soft", "Gate", "Late swell", "Dip", "Double", "Stutter"}; return n[i & 7]; }
inline float shapeAt(int i, float p) {
    switch (i & 7) {
        case 0: return std::exp(-p * 5.f);
        case 1: return 1.f - p;
        case 2: return 0.5f * (1.f + std::cos(3.14159265f * p));
        case 3: return p < 0.5f ? 1.f : 0.f;
        case 4: { const float u = std::min(1.f, std::max(0.f, (p - 0.25f) / 0.75f)); return p < 0.25f ? 1.f : 1.f - u * u * (3.f - 2.f * u); }
        case 5: return 1.f - std::fabs(2.f * p - 1.f);
        case 6: return std::exp(-std::fmod(p * 2.f, 1.f) * 5.f) * (p < 0.5f ? 1.f : 0.7f);
        default: return std::fmod(p * 4.f, 1.f) < 0.5f ? 1.f : 0.f;
    }
}

// mode 0 follow the source (tail dips while the source plays), 1 free (Hz), 2 BPM sync. src 0 none, 1 trigger signal given to tick().
struct DuckParams { float amt = 0, relMs = 250; int mode = 0, src = 1, shape = 0; float rateHz = 2, beats = 1, bpm = 120; };
class Duck {
public:
    void prepare(float sr) { sr_ = sr; env_ = 0; ph_ = 1; armed_ = true; gs_ = 1; cool_ = 0; dg = 1; }
    float tick(float dl, float dr, const DuckParams& p) {
        const float sr = sr_; float a = 0; bool trig = false;
        if (p.src == 1) {
            a = std::max(std::fabs(dl), std::fabs(dr));
            if (p.mode != 0) {
                const float ka = 1.f - std::exp(-1.f / (0.0005f * sr)), kr = 1.f - std::exp(-1.f / (0.04f * sr));
                env_ += (a > env_ ? ka : kr) * (a - env_);
                if (cool_ > 0) --cool_;
                if (armed_ && env_ > 0.08f && cool_ <= 0) { trig = true; armed_ = false; cool_ = int(0.05f * sr); }
                else if (!armed_ && env_ < 0.04f) armed_ = true;
            }
        }
        if (p.amt <= 0.0005f) { dg = 1; gs_ = 1; return 1; }
        if (p.mode == 0) {
            if (p.src == 0) { dg = 1; return 1; }
            const float kr = 1.f - std::exp(-1.f / (std::max(10.f, p.relMs) * 0.001f * sr)), ka = 1.f - std::exp(-1.f / (0.002f * sr));
            env_ += (a > env_ ? ka : kr) * (a - env_);
            dg = 1.f - p.amt * std::min(1.f, env_ * 3.f); return dg;
        }
        const float T = p.mode == 1 ? sr / std::max(0.05f, p.rateHz) : 60.f / std::max(30.f, p.bpm) * p.beats * sr;
        if (p.src == 1 ? trig : false) ph_ = 0;
        ph_ += 1.f / T;
        if (ph_ >= 1.f) { if (p.src == 0) ph_ -= 1.f; else ph_ = 1.f; }
        const float e = shapeAt(p.shape, std::min(1.f, ph_)), g = 1.f - p.amt * e;
        gs_ += (1.f - std::exp(-1.f / (0.0007f * sr))) * (g - gs_); dg = gs_; return gs_;
    }
    float dg = 1;
private:
    float sr_ = 48000, env_ = 0, ph_ = 1, gs_ = 1; bool armed_ = true; int cool_ = 0;
};

struct WetParams { float satDrive = 8, satMix = 0, squashDepth = 0.6f, squashMix = 0, width = 1, boostDb = 0; bool limit = false; DuckParams duck; };
class WetFx {
public:
    void prepare(float sr) { sr_ = sr; ad_ = Adaa(); l1_ = OnePole(); l2_ = OnePole(); for (auto& e : env_) e[0] = e[1] = 0; duck_.prepare(sr); }
    // wl, wr: the wet part. dl, dr: the trigger signal for the duck (the dry input or the sidechain).
    void process(float wl, float wr, float dl, float dr, const WetParams& p) {
        float w[2] = {wl, wr};
        for (int c = 0; c < 2; ++c) {
            float x = satStage(ad_, c, w[c], p.satDrive, p.satMix);
            if (p.squashMix > 0.0005f) { const float s = squash(c, x, p.squashDepth); x = x * (1.f - p.squashMix) + std::tanh(s) * p.squashMix; }
            else squash(c, x, 0.f);
            w[c] = x;
        }
        const float m = 0.5f * (w[0] + w[1]), sd = 0.5f * (w[0] - w[1]) * p.width; w[0] = m + sd; w[1] = m - sd;
        const float dg = duck_.tick(dl, dr, p.duck), bg = std::pow(10.f, p.boostDb / 20.f);
        oL = w[0] * dg * bg; oR = w[1] * dg * bg;
        if (p.limit) limiter(oL, oR); else limG_ = 1.f;
    }
    float oL = 0, oR = 0, gdb[3] = {0, 0, 0};
    const Duck& duck() const { return duck_; }
private:
    // Safety limiter at the very end of the wet chain (effect signal only): a fast stereo-linked gain limiter at -1 dBFS (0.3 ms attack, 120 ms release),
    // then a soft knee that only bends peaks above 0.9, so normal levels pass untouched. Off by default.
    void limiter(float& l, float& r) {
        const float pk = std::max(std::fabs(l), std::fabs(r)), thr = 0.891f, tgt = pk > thr ? thr / pk : 1.f;
        const float a = tgt < limG_ ? 1.f - std::exp(-1.f / (0.0003f * sr_)) : 1.f - std::exp(-1.f / (0.12f * sr_));
        limG_ += a * (tgt - limG_); l *= limG_; r *= limG_;
        auto soft = [](float x) { const float ax = std::fabs(x); if (ax <= 0.9f) return x; const float y = 0.9f + 0.1f * std::tanh((ax - 0.9f) / 0.1f); return x < 0.f ? -y : y; };
        l = soft(l); r = soft(r);
    }
    float limG_ = 1.f;
    float squash(int c, float x, float depth) {
        const float k1 = 1.f - std::exp(-6.2831853f * 130.f / sr_), k2 = 1.f - std::exp(-6.2831853f * 2800.f / sr_);
        const float lo = l1_.lp(c, x, k1), m2 = l2_.lp(c, x, k2), bands[3] = {lo, m2 - lo, x - m2};
        const float atk = 1.f - std::exp(-1.f / (0.004f * sr_)), rel = 1.f - std::exp(-1.f / (0.07f * sr_));
        float out = 0;
        for (int b = 0; b < 3; ++b) {
            const float a = std::fabs(bands[b]); float e = env_[b][c]; e += (a > e ? atk : rel) * (a - e); env_[b][c] = e;
            const float L = 20.f * std::log10(e + 1e-7f); float g = 0;
            if (L > -18.f) g = -(L + 18.f) * 0.6f; else if (L < -38.f) g = std::min(22.f, (-38.f - L) * 0.55f);
            g *= depth; if (c == 0) gdb[b] = g;
            out += bands[b] * std::pow(10.f, g / 20.f);
        }
        return out;
    }
    float sr_ = 48000; Adaa ad_; OnePole l1_, l2_; float env_[3][2] = {}; Duck duck_;
};

// ---------------- Modulation: chorus, flanger, phaser, tremolo, pan, rotary, vibrato, detune. Original code, textbook techniques.
// Unit style like the reverb and delay: the engine keeps the dry at (1 - mix) and adds this wet part.
struct ModParams {
    int type = 0;            // 0 Chorus, 1 Flanger, 2 Phaser, 3 Tremolo, 4 Pan, 5 Rotary, 6 Vibrato, 7 Detune
    float speed = 0.8f;      // Hz (Rotary: horn speed, kept between Min and Max speed)
    float depth = 0.5f;      // 0..1
    int voices = 2;          // Chorus: 1..4
    float regen = 0.0f;      // -0.9..0.9, Chorus and Flanger, Phaser: feedback
    float preDelayMs = 3.0f; // Flanger offset, Chorus base time add, Detune delay
    float diffusion = 0.0f;  // 0..1 Chorus: time smoothing
    int waveform = 0;        // 0 sine, 1 triangle, 2 soft square
    float phase = 0.5f;      // 0..1: left and right movement alike (0) or opposite (1)
    int stages = 1;          // Phaser: 0 = 4, 1 = 8, 2 = 12 stages
    float spread = 0.7f;     // Rotary: stereo spread 0..1
    float drive = 0.2f;      // Rotary: preamp drive 0..1
    float minHz = 0.8f, maxHz = 6.5f;   // Rotary speed limits
    float doppler = 0.5f;    // Rotary: pitch wobble 0..1
    float shift = 12.0f;     // Detune: cents (0..50)
    float mix = 1.0f;        // 1 for the wet only types, lower mixes the dry back in
};
constexpr int kModFx = 8;   // the effects run in this order: Chorus, Flanger, Phaser, Tremolo, Pan, Rotary, Vibrato, Detune; any of them can be on
inline bool modWetOnly(int type) { return type == 3 || type == 4 || type == 5 || type == 6; }
class ModFx {
public:
    void prepare(float sr) {
        sr_ = sr;
        for (int c = 0; c < 2; ++c) {
            dl_[c].prepare(int(0.08f * sr)); for (int k = 0; k < 2; ++k) { dif_[c][k].prepare(int((4.1f + 2.9f * k) * 0.001f * sr)); } hornDl_[c].prepare(int(0.01f * sr)); rotDl_[c].prepare(int(0.01f * sr));
            ps_[c][0].prepare(sr, 45.0f); ps_[c][1].prepare(sr, 45.0f);
        }
        reset();
    }
    void reset() {
        for (int c = 0; c < 2; ++c) { dl_[c].clear(); for (auto& a : dif_[c]) a.clear(); hornDl_[c].clear(); rotDl_[c].clear(); ps_[c][0].reset(); ps_[c][1].reset(); fb_[c] = 0; lo_[c] = 0; for (int k = 0; k < 12; ++k) { ax_[c][k] = 0; } }
        ph_ = 0; hornPh_ = rotPh_ = 0; hornHz_ = rotHz_ = 0.8f;
    }
    void process(float l, float r, const ModParams& p) {
        const float in[2] = {l, r}, twoPi = 6.2831853f; float o[2] = {l, r};
        ph_ += p.speed / sr_; ph_ -= std::floor(ph_);
        const float off = 0.5f * std::min(1.0f, std::max(0.0f, p.phase));
        const float depth = std::min(1.0f, std::max(0.0f, p.depth));
        switch (p.type) {
            case 0: {   // Chorus: up to 4 voices around a 8 ms + offset base, spread in phase
                const int nv = std::max(1, std::min(4, p.voices));
                for (int c = 0; c < 2; ++c) {
                    float sum = 0;
                    for (int k = 0; k < nv; ++k) {
                        const float lf = lfo(ph_ + c * off + float(k) / float(nv), p.waveform);
                        const float ms = 8.0f + p.preDelayMs + depth * 5.0f * (0.5f + 0.5f * lf) + 0.7f * float(k);
                        float d = dl_[c].read(std::max(2.0f, ms * 0.001f * sr_));
                        if (p.diffusion > 0.0f) { const float g = 0.15f + 0.5f * p.diffusion; for (auto& a : dif_[c]) d = a.tick(d, g); }
                        sum += d;
                    }
                    const float wet = sum / float(nv);
                    dl_[c].push(in[c] + std::max(-0.9f, std::min(0.9f, p.regen)) * fb_[c]); fb_[c] = wet;
                    o[c] = (in[c] + wet * 1.2f) * 0.6f;
                }
                break; }
            case 1: {   // Flanger: one short swept delay against the dry, with regen
                for (int c = 0; c < 2; ++c) {
                    const float lf = lfo(ph_ + c * off, p.waveform);
                    const float ms = std::max(0.2f, p.preDelayMs) + depth * 4.0f * (0.5f + 0.5f * lf);
                    const float d = dl_[c].read(std::max(2.0f, ms * 0.001f * sr_));
                    dl_[c].push(in[c] + std::max(-0.9f, std::min(0.9f, p.regen)) * d);
                    o[c] = 0.5f * (in[c] + d) * 1.2f;
                }
                break; }
            case 2: {   // Phaser: 4, 8 or 12 first order allpass stages swept in frequency
                const int n = p.stages <= 0 ? 4 : p.stages == 1 ? 8 : 12;
                for (int c = 0; c < 2; ++c) {
                    const float lf = lfo(ph_ + c * off, p.waveform), f = 250.0f * std::pow(2.0f, depth * 5.0f * (0.5f + 0.5f * lf));
                    const float tn = std::tan(3.14159265f * std::min(f, 0.45f * sr_) / sr_), a = (tn - 1.0f) / (tn + 1.0f);
                    float x = in[c] + std::max(-0.9f, std::min(0.9f, p.regen)) * fb_[c];
                    for (int k = 0; k < n; ++k) { const float y = a * x + ax_[c][k]; ax_[c][k] = x - a * y; x = y; }
                    fb_[c] = std::tanh(x); o[c] = 0.5f * (in[c] + x) * 1.3f;
                }
                break; }
            case 3: {   // Tremolo: both channels together (phase 0) to opposite (phase 1)
                for (int c = 0; c < 2; ++c) o[c] = in[c] * (1.0f - depth * (0.5f + 0.5f * lfo(ph_ + c * off, p.waveform)));
                break; }
            case 4: {   // Pan: the sound swings between left and right
                const float lf = lfo(ph_, p.waveform);
                o[0] = in[0] * (1.0f - depth * (0.5f + 0.5f * lf)); o[1] = in[1] * (1.0f - depth * (0.5f - 0.5f * lf));
                break; }
            case 5: {   // Rotary: horn (highs) and rotor (lows) spin the opposite way, with speed inertia, doppler, tremolo and pan
                const float tgt = std::min(p.maxHz, std::max(p.minHz, p.speed));
                hornHz_ += (1.0f - std::exp(-1.0f / (0.8f * sr_))) * (tgt - hornHz_); rotHz_ += (1.0f - std::exp(-1.0f / (1.8f * sr_))) * (tgt * 0.85f - rotHz_);
                hornPh_ += hornHz_ / sr_; rotPh_ -= rotHz_ / sr_; hornPh_ -= std::floor(hornPh_); rotPh_ -= std::floor(rotPh_);
                const float g = 1.0f + 6.0f * std::min(1.0f, std::max(0.0f, p.drive)), mono = std::tanh(0.5f * (in[0] + in[1]) * g) / std::tanh(g * 0.5f + 1e-3f) * 0.5f;
                const float kx = 1.0f - std::exp(-twoPi * 800.0f / sr_); lo_[0] += kx * (mono - lo_[0]); const float low = lo_[0], high = mono - lo_[0];
                const float hs = std::sin(twoPi * hornPh_), hc = std::cos(twoPi * hornPh_), rs = std::sin(twoPi * rotPh_), rc = std::cos(twoPi * rotPh_);
                const float dop = std::min(1.0f, std::max(0.0f, p.doppler)) * 0.0005f * sr_;
                hornDl_[0].push(high); rotDl_[0].push(low);
                const float hv = hornDl_[0].read(std::max(2.0f, 0.0006f * sr_ + dop * (1.0f + hs))), rv = rotDl_[0].read(std::max(2.0f, 0.0006f * sr_ + 0.4f * dop * (1.0f + rs)));
                const float sp = std::min(1.0f, std::max(0.0f, p.spread));
                const float hl = hv * (1.0f + 0.35f * hc) * (0.5f + 0.5f * sp * hs), hr = hv * (1.0f + 0.35f * hc) * (0.5f - 0.5f * sp * hs);
                const float rl = rv * (1.0f + 0.2f * rc) * (0.5f + 0.35f * sp * rs), rr = rv * (1.0f + 0.2f * rc) * (0.5f - 0.35f * sp * rs);
                o[0] = (hl + rl) * 1.8f; o[1] = (hr + rr) * 1.8f;
                break; }
            case 6: {   // Vibrato: the pitch swings around the original
                for (int c = 0; c < 2; ++c) {
                    const float lf = lfo(ph_ + c * off, p.waveform);
                    dl_[c].push(in[c]);
                    o[c] = dl_[c].read(std::max(2.0f, (3.0f + depth * 2.5f * (0.5f + 0.5f * lf)) * 0.001f * sr_));
                }
                break; }
            default: {   // Detune: a copy a few cents sharp on the left and flat on the right, after a short delay
                const float cents = std::min(50.0f, std::max(0.0f, p.shift)), up = std::pow(2.0f, cents / 1200.0f), dn = 1.0f / up;
                for (int c = 0; c < 2; ++c) {
                    dl_[c].push(in[c]);
                    const float x = p.preDelayMs > 0.5f ? dl_[c].read(p.preDelayMs * 0.001f * sr_) : in[c];
                    const float sh = ps_[c][0].tick(x, c ? dn : up);
                    o[c] = 0.5f * (x + sh) * 1.2f;
                }
                break; }
        }
        oL = o[0]; oR = o[1];
    }
    float oL = 0, oR = 0;
private:
    static float lfo(float ph, int wf) {
        ph -= std::floor(ph);
        if (wf == 1) return ph < 0.5f ? 4.0f * ph - 1.0f : 3.0f - 4.0f * ph;
        const float s = std::sin(6.2831853f * ph);
        return wf == 2 ? std::tanh(4.0f * s) / std::tanh(4.0f) : s;
    }
    float sr_ = 48000, ph_ = 0, hornPh_ = 0, rotPh_ = 0, hornHz_ = 0.8f, rotHz_ = 0.8f, fb_[2] = {}, lo_[2] = {}, ax_[2][12] = {};
    wm::engine::DelayLine dl_[2], hornDl_[2], rotDl_[2]; wm::engine::Allpass dif_[2][2]; wm::engine::PitchShifter ps_[2][2];
};

// ---------------- routing graph. "ifx>rev|del>wfx": stages left to right, names inside a stage run side by side.
enum Node { kIfx = 0, kRev = 1, kDel = 2, kWfx = 3, kMod = 4 };
constexpr int kNumNodes = 5;
inline const char* nodeId(int n) { static const char* s[kNumNodes] = {"ifx", "rev", "del", "wfx", "mod"}; return s[n % kNumNodes]; }
struct Graph {
    int count[kNumNodes] = {}, stages = 0;   // stages in use; stage s holds count[s] nodes in items[s][0..]
    int items[kNumNodes][kNumNodes] = {};
};
inline Graph parseGraph(const std::string& str) {
    Graph g; bool seen[kNumNodes] = {};
    size_t pos = 0;
    while (pos <= str.size() && g.stages < kNumNodes) {
        size_t end = str.find('>', pos); if (end == std::string::npos) end = str.size();
        const std::string chunk = str.substr(pos, end - pos); size_t q = 0; int n = 0;
        while (q <= chunk.size()) {
            size_t e2 = chunk.find('|', q); if (e2 == std::string::npos) e2 = chunk.size();
            const std::string id = chunk.substr(q, e2 - q);
            for (int k = 0; k < kNumNodes; ++k) if (id == nodeId(k) && !seen[k] && n < kNumNodes) { seen[k] = true; g.items[g.stages][n++] = k; }
            q = e2 + 1;
        }
        if (n > 0) g.count[g.stages++] = n;
        pos = end + 1;
    }
    for (int k = 0; k < kNumNodes; ++k) if (!seen[k] && g.stages < kNumNodes) { g.items[g.stages][0] = k; g.count[g.stages++] = 1; }
    return g;
}
inline std::string graphString(const Graph& g) {
    std::string s;
    for (int i = 0; i < g.stages; ++i) { if (i) s += '>'; for (int j = 0; j < g.count[i]; ++j) { if (j) s += '|'; s += nodeId(g.items[i][j]); } }
    return s;
}

struct GState { float dl = 0, dr = 0, xl = 0, xr = 0, wl = 0, wr = 0; bool hw = false; };

struct Engine {
    wm::engine::ReverbUnit rev; wm::engine::ShimmerDelay del; InFx ifx; WetFx wfx; ModFx mods[kModFx];
    // per block settings
    wm::engine::ReverbSettings rs; wm::engine::ShimmerDelayParams dp; InParams ip; WetParams wp; ModParams mps[kModFx]; bool modUse[kModFx] = {true}; float modMix = 1.0f, modLevel = 1.0f;
    bool revOn = true, delOn = false, ifxOn = true, wfxOn = true, modOn = false;
    void prepare(float sr) { rev.prepare(sr); del.prepare(sr); ifx.prepare(sr); wfx.prepare(sr); for (auto& m : mods) m.prepare(sr); }
    void reset() { rev.reset(); del.reset(); for (auto& m : mods) m.reset(); }
    // One sample. l, r: dry input. tl, tr: duck trigger (the dry input, or the sidechain). Returns output in ol, or.
    void tick(const Graph& g, float l, float r, float tl, float tr, float& ol, float& orr) {
        GState a; a.dl = a.xl = l; a.dr = a.xr = r;
        for (int s = 0; s < g.stages; ++s) {
            GState o;
            if (g.count[s] == 1) o = node(g.items[s][0], a, tl, tr);
            else {
                GState acc; const int n = g.count[s];
                for (int k = 0; k < n; ++k) {
                    const GState t = node(g.items[s][k], a, tl, tr);
                    acc.dl += t.dl; acc.dr += t.dr; acc.xl += t.xl; acc.xr += t.xr; acc.wl += t.wl; acc.wr += t.wr; acc.hw = acc.hw || t.hw;
                }
                const float q = 1.f / float(n);
                acc.dl *= q; acc.dr *= q; acc.xl *= q; acc.xr *= q; acc.wl *= q; acc.wr *= q; o = acc;
            }
            a = o;
        }
        ol = a.dl + a.wl; orr = a.dr + a.wr;
        if (!std::isfinite(ol) || !std::isfinite(orr) || std::fabs(ol) > 50.f || std::fabs(orr) > 50.f) { reset(); ol = orr = 0; }
    }
private:
    // Each unit makes out = unit(x + w) and keeps x*(1-mix) as its dry; the wet part is what is left of the output.
    GState node(int id, const GState& a, float tl, float tr) {
        GState o = a;
        if (id == kIfx) {
            if (!ifxOn) return o;
            ifx.process(a.xl + a.wl, a.xr + a.wr, ip);
            if (!a.hw) { o.xl = ifx.oL; o.xr = ifx.oR; o.wl = 0; o.wr = 0; } else { o.wl = ifx.oL - a.xl; o.wr = ifx.oR - a.xr; }
        } else if (id == kRev) {
            if (!revOn) return o;
            const float g = 1.f - rs.mix; float ul, ur; rev.process(a.xl + a.wl, a.xr + a.wr, ul, ur, rs);
            o.dl = a.dl * g; o.dr = a.dr * g; o.xl = a.xl * g; o.xr = a.xr * g; o.wl = ul - o.xl; o.wr = ur - o.xr; o.hw = true;
        } else if (id == kDel) {
            if (!delOn) return o;
            const float g = 1.f - dp.mix; float ul, ur; del.process(a.xl + a.wl, a.xr + a.wr, ul, ur, dp);
            o.dl = a.dl * g; o.dr = a.dr * g; o.xl = a.xl * g; o.xr = a.xr * g; o.wl = ul - o.xl; o.wr = ur - o.xr; o.hw = true;
        } else if (id == kMod) {
            if (!modOn) return o;
            float xl = a.xl + a.wl, xr = a.xr + a.wr; int n = 0;
            for (int k = 0; k < kModFx; ++k) if (modUse[k]) { mods[k].process(xl, xr, mps[k]); xl = mods[k].oL; xr = mods[k].oR; ++n; }
            if (n == 0) return o;
            const float g = 1.f - modMix;
            o.dl = a.dl * g; o.dr = a.dr * g; o.xl = a.xl * g; o.xr = a.xr * g; o.wl = (xl - o.xl) * modLevel; o.wr = (xr - o.xr) * modLevel; o.hw = true;
        } else if (wfxOn) {
            wfx.process(a.wl, a.wr, tl, tr, wp); o.wl = wfx.oL; o.wr = wfx.oR;
        }
        return o;
    }
};

}  // namespace wm::fx
