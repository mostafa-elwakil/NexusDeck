/**
 * Regression tests for the boot-sync fix:
 * passive profile loads (boot, remote auto-sync) must PULL from the server
 * and never PUSH a stale local cache over server state.
 * Run with:  node --test tests/profiles.test.js
 */
const { describe, it, beforeEach } = require('node:test');
const assert = require('node:assert/strict');

// ---- Browser shims ----
const store = new Map();
global.localStorage = {
    getItem: (key) => (store.has(key) ? store.get(key) : null),
    setItem: (key, value) => store.set(key, String(value)),
    removeItem: (key) => store.delete(key),
    clear: () => store.clear(),
};
if (typeof global.CustomEvent === 'undefined') {
    global.CustomEvent = class CustomEvent {
        constructor(type, options = {}) {
            this.type = type;
            this.detail = options.detail;
        }
    };
}

const fetchCalls = [];
let serverProfile = null;
global.fetch = async (url, options = {}) => {
    fetchCalls.push({ url, options });
    if (url.endsWith('/api/get-profile')) {
        return { ok: true, json: async () => JSON.parse(JSON.stringify(serverProfile)) };
    }
    if (url.endsWith('/api/set-profile')) {
        return { ok: true, json: async () => ({ ok: true }) };
    }
    return { ok: false, status: 404, json: async () => ({}) };
};

const ProfilesManager = require('../streamdeck-simulator/js/profiles.js');

function makeManager() {
    const dispatched = [];
    const deck = {
        config: { size: 'cyd' },
        container: { dispatchEvent: (event) => dispatched.push(event.type) },
        getButtonCount: () => 12,
        updateButton: () => {},
        resize: () => {},
        exportProfile: () => ({ size: 'cyd', rows: 3, cols: 4, buttons: [] }),
    };
    const actions = { serverUrl: 'http://localhost:8765' };
    const liveKeys = { stopAll: () => {}, registerWidget: () => {} };
    const manager = new ProfilesManager(deck, actions, liveKeys);
    return { manager, dispatched };
}

const flush = async (rounds = 5) => {
    for (let i = 0; i < rounds; i += 1) {
        await new Promise((resolve) => setImmediate(resolve));
    }
};

const setPosts = () => fetchCalls.filter((call) => call.url.endsWith('/api/set-profile'));

describe('boot / passive sync', () => {
    beforeEach(() => {
        store.clear();
        fetchCalls.length = 0;
        serverProfile = null;
    });

    it('boot with unreachable server pushes nothing', async () => {
        global.fetch = async () => { throw new Error('offline'); };
        try {
            makeManager();
            await flush();
            assert.equal(setPosts().length, 0);
        } finally {
            delete global.fetch;
            global.fetch = async (url, options = {}) => {
                fetchCalls.push({ url, options });
                if (url.endsWith('/api/get-profile')) {
                    return { ok: true, json: async () => JSON.parse(JSON.stringify(serverProfile)) };
                }
                return { ok: true, json: async () => ({ ok: true }) };
            };
        }
    });

    it('explicit loadProfile() pushes to the server', async () => {
        const { manager } = makeManager();
        await flush();
        fetchCalls.length = 0;
        manager.loadProfile(manager.profiles[0]);
        await flush();
        assert.equal(setPosts().length, 1);
    });

    it('loadProfile() with push:false never pushes', async () => {
        const { manager } = makeManager();
        await flush();
        fetchCalls.length = 0;
        manager.loadProfile(manager.profiles[0], { push: false });
        await flush();
        assert.equal(setPosts().length, 0);
    });

    it('boot pull adopts newer server buttons without pushing', async () => {
        serverProfile = {
            name: 'Server Truth',
            buttons: [{ label: 'FromServer', icon: '', color: '#ffffff', action: null }],
        };
        const { manager } = makeManager();
        await flush();
        const adopted = manager.getProfile('Server Truth');
        assert.ok(adopted, 'server profile should be adopted locally');
        assert.equal(adopted.buttons[0].label, 'FromServer');
        assert.equal(setPosts().length, 0, 'pull must not push back');
    });

    it('boot pull with identical state pushes nothing and adds no duplicate', async () => {
        const { manager } = makeManager();
        await flush();
        const current = manager.currentProfile;
        serverProfile = JSON.parse(JSON.stringify(current));
        const before = manager.profiles.length;
        fetchCalls.length = 0;
        await manager.pullServerProfile();
        assert.equal(manager.profiles.length, before);
        assert.equal(setPosts().length, 0);
    });
});
