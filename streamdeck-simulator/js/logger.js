/**
 * Bounded client-side logger with configurable levels and export support.
 */
class AppLogger {
    constructor(options = {}) {
        this.levels = { debug: 10, info: 20, warn: 30, error: 40 };
        this.level = options.level || 'info';
        this.maxEntries = options.maxEntries || 200;
        this.entries = [];
    }

    setLevel(level) {
        if (!(level in this.levels)) {
            throw new Error(`Unknown log level: ${level}`);
        }
        this.level = level;
    }

    write(level, message, context = {}) {
        if (this.levels[level] < this.levels[this.level]) {
            return;
        }

        const entry = {
            timestamp: new Date().toISOString(),
            level,
            message: String(message),
            context
        };
        this.entries.push(entry);
        if (this.entries.length > this.maxEntries) {
            this.entries.splice(0, this.entries.length - this.maxEntries);
        }

        const consoleMethod = console[level] || console.log;
        consoleMethod(`[${level.toUpperCase()}] ${entry.message}`, context);

        if (typeof document !== 'undefined') {
            document.dispatchEvent(new CustomEvent('app:log', { detail: entry }));
        }
    }

    debug(message, context) {
        this.write('debug', message, context);
    }

    info(message, context) {
        this.write('info', message, context);
    }

    warn(message, context) {
        this.write('warn', message, context);
    }

    error(message, context) {
        this.write('error', message, context);
    }

    getEntries(limit = this.maxEntries) {
        return this.entries.slice(-Math.max(0, limit)).reverse();
    }

    clear() {
        this.entries = [];
    }

    exportJson() {
        return JSON.stringify(this.entries, null, 2);
    }

    destroy() {
        this.clear();
    }
}

const appLogger = new AppLogger();

if (typeof module !== 'undefined' && module.exports) {
    module.exports = { AppLogger, appLogger };
}
