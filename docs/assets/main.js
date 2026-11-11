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
