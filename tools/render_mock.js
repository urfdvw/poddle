// Renders docs/watchface_mockup.html at a fixed time, at 1x:
//   OUT_portrait_hour.png    Frame 2 (portrait, progress within hour)
//   OUT_portrait_minute.png  Frame 1 (portrait, progress within minute)
//   OUT_screen.png           Frame 3 as the physical 144x168 screen
//   OUT_canvas.png           Frame 3 as the unrotated 168x144 canvas
//   node tools/render_mock.js "2026-10-01T15:29:18" out_prefix
const path = require('path');
const { chromium } = require(process.env.PLAYWRIGHT_MODULE || 'playwright');

(async () => {
  const when = process.argv[2] || '2026-10-01T15:29:18';
  const out = process.argv[3] || 'mock';
  const browser = await chromium.launch({ executablePath: process.env.CHROMIUM || undefined });
  const page = await browser.newPage({ deviceScaleFactor: 1, viewport: { width: 1400, height: 900 } });
  await page.addInitScript((iso) => {
    const fixed = new Date(iso).getTime();
    const RealDate = Date;
    // eslint-disable-next-line no-global-assign
    Date = class extends RealDate {
      constructor(...a) { super(...(a.length ? a : [fixed])); }
      static now() { return fixed; }
    };
  }, when);
  await page.goto('file://' + path.resolve(__dirname, '../docs/watchface_mockup.html'));
  await page.waitForTimeout(200);
  for (const [index, name] of [[1, 'hour'], [0, 'minute']]) {
    // Clone into a pixel-aligned host so the shot is exactly 144x168.
    await page.evaluate((i) => {
      const el = document.querySelectorAll('.screen-content')[i].cloneNode(true);
      el.style.boxShadow = 'none'; el.style.borderRadius = '0';
      const host = document.createElement('div');
      host.id = 'portrait-host';
      host.style.cssText = 'position:fixed;left:0;top:0;width:144px;height:168px;overflow:hidden;z-index:99';
      host.appendChild(el);
      document.body.appendChild(host);
    }, index);
    await page.locator('#portrait-host').screenshot({ path: `${out}_portrait_${name}.png` });
    await page.evaluate(() => document.getElementById('portrait-host').remove());
  }
  await page.locator('.screen-physical-rot').screenshot({ path: out + '_screen.png' });
  await page.evaluate(() => {
    const el = document.querySelector('.screen-content-rot');
    const host = document.createElement('div');
    host.id = 'canvas-host';
    host.style.cssText = 'position:fixed;left:0;top:0;width:168px;height:144px;overflow:hidden;z-index:99';
    el.style.transform = 'none'; el.style.top = '0'; el.style.left = '0';
    host.appendChild(el);
    document.body.appendChild(host);
  });
  await page.locator('#canvas-host').screenshot({ path: out + '_canvas.png' });
  await browser.close();
})();
