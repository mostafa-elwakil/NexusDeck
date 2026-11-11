# NexusDeck Website

A static marketing landing page for NexusDeck — plain HTML/CSS with a tiny bit of
progressive-enhancement JavaScript. No build step, no frameworks, no dependencies.

## Files

```
website/
├── index.html        # the landing page (navbar → hero → features → how-it-works → downloads → FAQ → footer)
├── assets/
│   ├── style.css     # all styling; palette via CSS variables (dark navy + teal #008080 accent)
│   └── main.js       # optional: reveal-on-scroll + mock device clock; page works fine with JS disabled
└── README.md
```

## Publish to GitHub Pages

GitHub Pages serves static files straight from your repo — pick **one** of these options:

### Option A — deploy from `main` (root or `/docs`)

1. Go to your repo on GitHub → **Settings** → **Pages** (under "Code and automation").
2. Under **Build and deployment** → **Source**, choose **Deploy from a branch**.
3. Pick branch `main` and folder:
   - `/docs` — if you prefer to keep the repo root clean. Copy the contents of
     `website/` into a `docs/` folder and commit those instead (like this:
     `xcopy /E /I website docs` on Windows, or `cp -r website/* docs/` on Linux).
   - `/ (root)` — only sensible if the repo root *is* the site.
4. Click **Save**. After a minute the site is live at
   `https://mostafa-elwakil.github.io/NexusDeck/`.

### Option B — dedicated `gh-pages` branch (recommended)

Keeps the repo root untouched:

```sh
git subtree push --prefix website origin gh-pages
```

Then in **Settings → Pages**, set Source = **Deploy from a branch**, branch =
`gh-pages`, folder `/ (root)`.

### Option C — GitHub Actions

Create `.github/workflows/pages.yml`:

```yaml
name: Deploy website to Pages
on:
  push:
    branches: [main]
    paths: ["website/**"]
permissions:
  pages: write
  contents: read
  id-token: write
concurrency:
  group: pages
  cancel-in-progress: true
jobs:
  deploy:
    environment:
      name: github-pages
      url: ${{ steps.deployment.outputs.page_url }}
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - uses: actions/configure-pages@v5
      - uses: actions/upload-pages-artifact@v3
        with:
          path: website
      - id: deployment
        uses: actions/deploy-pages@v4
```

In **Settings → Pages**, set Source = **GitHub Actions**. Every push that touches
`website/` redeploys.

## Test locally

```sh
cd website
python -m http.server 8080
```

Open http://localhost:8080 — the page is fully self-contained, so even just
double-clicking `index.html` works offline.

## Sitemap-lite

| URL | Purpose |
| --- | --- |
| `/` (#top) | Landing page |
| `/#features` | Feature grid |
| `/#setup` | How it works / setup steps |
| `/#download` | Downloads (Windows, firmware, Linux) |
| `/#faq` | FAQ |
| `/#faq-details` | Collapsible answers (in-page) |
| GitHub → Releases | Windows installer + ESP32 firmware binaries |
| GitHub → `tools/linux/install-server.sh` | Linux companion install script |

## Notes

- Fonts use system stacks only — nothing is fetched from the network.
- The Arabic FAQ line is `dir="rtl"` and `lang="ar"` for correct rendering.
- Colors: background `#101320`, accent teal `#008080` (matches the LED matrix
  default color).
