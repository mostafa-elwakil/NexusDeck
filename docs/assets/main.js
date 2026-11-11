/* NexusDeck landing page — progressive enhancement only */

/* Reveal-on-scroll (falls back to visible via CSS if JS is off) */
(function () {
    if (!("IntersectionObserver" in window)) {
        document.querySelectorAll(".reveal").forEach(function (el) {
            el.classList.add("visible");
        });
        return;
    }

    var observer = new IntersectionObserver(function (entries) {
        entries.forEach(function (entry) {
            if (entry.isIntersecting) {
                entry.target.classList.add("visible");
                observer.unobserve(entry.target);
            }
        });
    }, { threshold: 0.12 });

    document.querySelectorAll(".reveal").forEach(function (el) {
        observer.observe(el);
    });
})();

/* Mock device clock ticks to feel alive */
(function () {
    var clock = document.getElementById("mock-clock");
    if (!clock) return;

    function tick() {
        var now = new Date();
        clock.textContent = String(now.getHours()).padStart(2, "0") + ":" + String(now.getMinutes()).padStart(2, "0");
    }

    tick();
    setInterval(tick, 10000);
})();

/* Direct-download resolver: point download buttons at the newest release
 * asset without sending the visitor to the releases page. Buttons keep a
 * stable releases/latest/download/<fixed-name> href as the no-JS fallback;
 * those fixed names only exist from the next tag on, so until then we swap
 * them for the current release's versioned asset names via the API. */
(function () {
    var MATCH = {
        installer: /^NexusDeck-Setup-.*-windows-x64\.exe$/,
        firmware: /^firmware-esp32-.*\.bin$/,
        linux: /^NexusDeck-.*-linux-x64\.tar\.gz$/
    };

    if (typeof window.fetch !== "function") return;

    fetch("https://api.github.com/repos/mostafa-elwakil/NexusDeck/releases/latest", {
        headers: { "Accept": "application/vnd.github+json" }
    }).then(function (res) {
        return res.ok ? res.json() : null;
    }).then(function (rel) {
        if (!rel || !Array.isArray(rel.assets)) return;

        document.querySelectorAll("a[data-download]").forEach(function (a) {
            var re = MATCH[a.getAttribute("data-download")];
            if (!re) return;

            var picks = rel.assets.filter(function (asset) {
                return re.test(asset.name) && asset.browser_download_url;
            });
            // Prefer the fixed "-latest-" asset, fall back to versioned names.
            var pick = picks.filter(function (asset) {
                return asset.name.indexOf("-latest-") !== -1;
            })[0] || picks[0];

            if (pick) a.setAttribute("href", pick.browser_download_url);
        });
    }).catch(function () {
        /* Offline or rate-limited: the fallback href stays in place. */
    });
})();
