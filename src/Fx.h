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

struct WetParams { float satDrive = 8, satMix = 0, squashDepth = 0.6f, squashMix = 0, width = 1, boostDb = 0; DuckParams duck; };
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
    }
    float oL = 0, oR = 0, gdb[3] = {0, 0, 0};
    const Duck& duck() const { return duck_; }
private:
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

// ---------------- routing graph. "ifx>rev|del>wfx": stages left to right, names inside a stage run side by side.
enum Node { kIfx = 0, kRev = 1, kDel = 2, kWfx = 3 };
inline const char* nodeId(int n) { static const char* s[4] = {"ifx", "rev", "del", "wfx"}; return s[n & 3]; }
struct Graph {
    int count[4] = {0, 0, 0, 0}, stages = 0;   // stages in use; stage s holds count[s] nodes in items[s][0..]
    int items[4][4] = {};
};
inline Graph parseGraph(const std::string& str) {
    Graph g; bool seen[4] = {false, false, false, false};
    size_t pos = 0;
    while (pos <= str.size() && g.stages < 4) {
        size_t end = str.find('>', pos); if (end == std::string::npos) end = str.size();
        const std::string chunk = str.substr(pos, end - pos); size_t q = 0; int n = 0;
        while (q <= chunk.size()) {
            size_t e2 = chunk.find('|', q); if (e2 == std::string::npos) e2 = chunk.size();
            const std::string id = chunk.substr(q, e2 - q);
            for (int k = 0; k < 4; ++k) if (id == nodeId(k) && !seen[k] && n < 4) { seen[k] = true; g.items[g.stages][n++] = k; }
            q = e2 + 1;
        }
        if (n > 0) g.count[g.stages++] = n;
        pos = end + 1;
    }
    for (int k = 0; k < 4; ++k) if (!seen[k] && g.stages < 4) { g.items[g.stages][0] = k; g.count[g.stages++] = 1; }
    return g;
}
inline std::string graphString(const Graph& g) {
    std::string s;
    for (int i = 0; i < g.stages; ++i) { if (i) s += '>'; for (int j = 0; j < g.count[i]; ++j) { if (j) s += '|'; s += nodeId(g.items[i][j]); } }
    return s;
}

struct GState { float dl = 0, dr = 0, xl = 0, xr = 0, wl = 0, wr = 0; bool hw = false; };

struct Engine {
    wm::engine::ReverbUnit rev; wm::engine::ShimmerDelay del; InFx ifx; WetFx wfx;
    // per block settings
    wm::engine::ReverbSettings rs; wm::engine::ShimmerDelayParams dp; InParams ip; WetParams wp;
    bool revOn = true, delOn = false, ifxOn = true, wfxOn = true;
    void prepare(float sr) { rev.prepare(sr); del.prepare(sr); ifx.prepare(sr); wfx.prepare(sr); }
    void reset() { rev.reset(); del.reset(); }
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
        } else if (wfxOn) {
            wfx.process(a.wl, a.wr, tl, tr, wp); o.wl = wfx.oL; o.wr = wfx.oR;
        }
        return o;
    }
};

}  // namespace wm::fx
