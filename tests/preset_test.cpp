// Regression test: the Reverb and Delay tabs are only editing views. Their presets must never change the routing,
// and each tab's preset set carries its unit's on/off switch (same parameter as the routing power icon).
#include "../src/Presets.h"
#include <cstdio>
#include <cstring>
static int fails = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL line %d: %s\n", __LINE__, #c); ++fails; } } while (0)
static bool has(const std::vector<const char*>& k, const char* id) { for (auto* s : k) if (!std::strcmp(s, id)) return true; return false; }
int main()
{
    CHECK(has(ssp::reverbKeys(), "rev_on") && !has(ssp::reverbKeys(), "del_on") && !has(ssp::reverbKeys(), "route"));
    CHECK(has(ssp::delayKeys(), "del_on") && !has(ssp::delayKeys(), "rev_on") && !has(ssp::delayKeys(), "route"));
    CHECK(has(ssp::fxKeys(), "route") && has(ssp::fxKeys(), "rev_on") && has(ssp::fxKeys(), "del_on"));
    for (const auto* list : { &ssp::reverbPresets(), &ssp::delayPresets() })
        for (const auto& p : *list)
        {
            CHECK(p.graph == nullptr);                                   // a tab preset never sets the routing graph
            for (const auto& kv : p.v) CHECK(std::strcmp(kv.first, "route") != 0);
        }
    for (const auto& p : ssp::reverbPresets()) for (const auto& kv : p.v) CHECK(std::strcmp(kv.first, "del_on") != 0);
    for (const auto& p : ssp::delayPresets()) for (const auto& kv : p.v) CHECK(std::strcmp(kv.first, "rev_on") != 0);
    if (!fails) std::printf("preset_test OK\n");
    return fails ? 1 : 0;
}
