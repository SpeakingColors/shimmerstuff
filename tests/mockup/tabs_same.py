import asyncio, sys, json
from playwright.async_api import async_playwright
URL = sys.argv[1]
JS = """
async (mode) => {
  document.querySelector('button[data-mode="%s"]'.replace('%s', mode)).click();
  await new Promise(r => setTimeout(r, 300));
  if (!E.node) ensureAudio();
  E.node.onaudioprocess = null;               // stop the live callback so only the test drives the engine
  E.clear = true;
  let seed = 12345; const rnd = () => (seed = (seed * 1664525 + 1013904223) >>> 0) / 4294967296 - 0.5;
  const N = 2048, mk = () => { const a = [new Float32Array(N), new Float32Array(N)]; return { getChannelData: c => a[c], length: N }; };
  let h = 0, energy = 0;
  for (let b = 0; b < 60; b++) {
    const i = mk(), o = mk();
    for (let k = 0; k < N; k++) { const v = b < 3 ? rnd() * 0.8 : 0; i.getChannelData(0)[k] = v; i.getChannelData(1)[k] = v * 0.9; }
    procBlock({ inputBuffer: i, outputBuffer: o });
    const L = o.getChannelData(0), R = o.getChannelData(1);
    for (let k = 0; k < N; k++) { energy += L[k] * L[k] + R[k] * R[k]; h = (h * 31 + Math.round(L[k] * 1e6) + 7 * Math.round(R[k] * 1e6)) | 0; }
  }
  return { mode, h, energy, rev: S.rev_on, del: S.del_on, graph: S.graph };
}
"""
async def main():
    async with async_playwright() as p:
        b = await p.chromium.launch(args=["--autoplay-policy=no-user-gesture-required"])
        pg = await b.new_page(viewport={"width": 1500, "height": 1100})
        errs = []; pg.on("pageerror", lambda e: errs.append(str(e)))
        await pg.goto("file://" + URL); await pg.wait_for_timeout(600)
        await pg.click("#bPlay"); await pg.wait_for_timeout(300); await pg.click("#bPlay")
        # routing: reverb into delay, both on
        await pg.evaluate("() => { setParam('graph','ifx>rev>del>wfx',true); setParam('rev_on',true,true); setParam('del_on',true,true); }")
        res = [await pg.evaluate(JS, m) for m in ["reverb", "delay", "fx", "reverb"]]
        print(json.dumps(res))
        assert all(r["h"] == res[0]["h"] for r in res), "audio differs between tabs"
        assert res[0]["energy"] > 1e-3, "no audio"
        # both units really process: bypassing either changes the sound
        async def run(rev, dl):
            await pg.evaluate(f"() => {{ setParam('rev_on',{str(rev).lower()},true); setParam('del_on',{str(dl).lower()},true); }}")
            return (await pg.evaluate(JS, "reverb"))["h"]
        both, rev_only, del_only = res[0]["h"], await run(True, False), await run(False, True)
        assert len({both, rev_only, del_only}) == 3, "units are not independent"
        # the ON buttons on each tab drive the same state as the routing power icons
        await pg.click('button[data-mode="reverb"]'); await pg.wait_for_timeout(200)
        await pg.evaluate("() => setParam('rev_on', true, true)")
        btn = pg.locator('#fxR button.pwr'); assert await btn.count() == 1
        await btn.click(); assert await pg.evaluate("() => S.rev_on") is False
        await pg.click('button[data-mode="delay"]'); await pg.wait_for_timeout(200)
        dbtn = pg.locator('#fxD button.pwr'); assert await dbtn.count() == 1
        before = await pg.evaluate("() => S.del_on"); await dbtn.click()
        assert await pg.evaluate("() => S.del_on") == (not before)
        assert not errs, errs
        print("OK")
        await b.close()
asyncio.run(main())
